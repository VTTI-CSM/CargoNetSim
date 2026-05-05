#include "ScenarioExecutor.h"

#include <exception>
#include <QThread>
#include <QUuid>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "BufferedExecutionEventQueue.h"
#include "ContainerAllocator.h"
#include "DispatchableWaveBuilder.h"
#include "ExecutionEventAdapter.h"
#include "ExecutionPlanBuilder.h"
#include "ExecutionProgressCalculator.h"
#include "NetworkExecutionSessionManager.h"
#include "PathExecutionCoordinator.h"
#include "ResultsExtractor.h"
#include "ScenarioDocument.h"
#include "ScenarioRegistry.h"
#include "SegmentCostMath.h"
#include "SimulationSettings.h"
#include "SimulatorCommandAvailability.h"
#include "TerminalGraphBootstrap.h"
#include "TerminalInventoryGateway.h"
#include "TerminalPickupCoordinator.h"

#include <QElapsedTimer>
#include <algorithm>
#include <optional>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

constexpr int kPostArrivalEventWaitMs = 5000;

struct ExecutableSelection
{
    QList<CargoNetSim::Backend::Path *> paths;
    QVector<QString>                    executionPathKeys;
};

struct LiveExecutionTimeOverrideGuard
{
    TrainClient::TrainSimulationClient *trainClient = nullptr;
    ShipClient::ShipSimulationClient   *shipClient = nullptr;

    void set(double currentTimeSeconds) const
    {
        if (trainClient)
            trainClient->setExecutionTimeOverride(currentTimeSeconds);
        if (shipClient)
            shipClient->setExecutionTimeOverride(currentTimeSeconds);
    }

    ~LiveExecutionTimeOverrideGuard()
    {
        if (trainClient)
            trainClient->clearExecutionTimeOverride();
        if (shipClient)
            shipClient->clearExecutionTimeOverride();
    }
};

QString dispositionLabel(PlannedPathDisposition disposition)
{
    switch (disposition)
    {
    case PlannedPathDisposition::Execute:
        return QStringLiteral("Execute");
    case PlannedPathDisposition::SkipNoDemand:
        return QStringLiteral("SkipNoDemand");
    case PlannedPathDisposition::SkipByPolicy:
        return QStringLiteral("SkipByPolicy");
    case PlannedPathDisposition::PlanningFailure:
        return QStringLiteral("PlanningFailure");
    }

    return QStringLiteral("Unknown");
}

QString schedulingPolicyLabel(ExecutionSchedulingPolicy policy)
{
    switch (policy)
    {
    case ExecutionSchedulingPolicy::ConcurrentPaths:
        return QStringLiteral("ConcurrentPaths");
    case ExecutionSchedulingPolicy::SequentialPaths:
        return QStringLiteral("SequentialPaths");
    }

    return QStringLiteral("Unknown");
}

QString isolationPolicyLabel(ExecutionIsolationPolicy policy)
{
    switch (policy)
    {
    case ExecutionIsolationPolicy::SharedSimulatorState:
        return QStringLiteral("SharedSimulatorState");
    case ExecutionIsolationPolicy::IsolatedAlternatives:
        return QStringLiteral("IsolatedAlternatives");
    }

    return QStringLiteral("Unknown");
}

bool pathUsesMode(const CargoNetSim::Backend::Path *path,
                  TransportationTypes::TransportationMode mode)
{
    if (!path)
        return false;

    for (const auto *segment : path->getSegments())
    {
        if (segment && segment->getMode() == mode)
            return true;
    }
    return false;
}

bool resetModeStateForAlternative(
    CargoNetSim::CargoNetSimController       &controller,
    const CargoNetSim::Backend::Path         *path,
    QString                                  *err)
{
    using Mode = TransportationTypes::TransportationMode;

    if (pathUsesMode(path, Mode::Train))
    {
        auto *trainClient = controller.getTrainClient();
        if (!isCommandAvailable(trainClient))
        {
            if (err)
                *err = QStringLiteral(
                    "Train command queue is unavailable for isolated what-if path");
            return false;
        }
        if (!trainClient->resetServer())
        {
            if (err)
                *err = QStringLiteral(
                    "Failed to reset NeTrainSim for isolated what-if path");
            return false;
        }
    }

    if (pathUsesMode(path, Mode::Ship))
    {
        auto *shipClient = controller.getShipClient();
        if (!isCommandAvailable(shipClient))
        {
            if (err)
                *err = QStringLiteral(
                    "Ship command queue is unavailable for isolated what-if path");
            return false;
        }
        if (!shipClient->resetServer())
        {
            if (err)
                *err = QStringLiteral(
                    "Failed to reset ShipNetSim for isolated what-if path");
            return false;
        }
    }

    if (pathUsesMode(path, Mode::Truck))
    {
        auto *truckManager = controller.getTruckManager();
        if (!truckManager)
        {
            if (err)
                *err = QStringLiteral(
                    "Truck manager is unavailable for isolated what-if path");
            return false;
        }
        if (!truckManager->resetServer())
        {
            if (err)
                *err = QStringLiteral(
                    "Failed to reset truck simulations for isolated what-if path");
            return false;
        }
    }

    if (err)
        err->clear();
    return true;
}

double clampProgressPercent(double value)
{
    return std::max(0.0, std::min(100.0, value));
}

void copyActiveSegmentFields(PathProgressSnapshot *row)
{
    if (!row
        || row->activeSegmentIndex < 0
        || row->activeSegmentIndex >= row->segments.size())
    {
        return;
    }

    const auto &segment = row->segments.at(row->activeSegmentIndex);
    row->activeMode = segment.mode;
    row->activeNetworkName = segment.networkName;
    row->activeStartTerminalId = segment.startTerminalId;
    row->activeEndTerminalId = segment.endTerminalId;
}

void markProgressRowCompleted(PathProgressSnapshot *row)
{
    if (!row)
        return;

    row->lifecycle = PathLifecycleState::Completed;
    row->percent = 100.0;
    row->completedSegments = row->totalSegments;
    row->activeSegmentIndex =
        row->totalSegments > 0 ? row->totalSegments - 1 : -1;

    for (auto &segment : row->segments)
    {
        segment.lifecycle = SegmentLifecycleState::Completed;
        segment.percent = 100.0;
        segment.active = false;
        if (segment.dispatchedVehicleCount > 0)
        {
            segment.completedVehicleCount =
                std::max(segment.completedVehicleCount,
                         segment.dispatchedVehicleCount);
        }
    }

    copyActiveSegmentFields(row);
}

QHash<QString, int> indexProgressRowsByExecutionPathKey(
    const QVector<PathProgressSnapshot> &rows)
{
    QHash<QString, int> index;
    for (int i = 0; i < rows.size(); ++i)
    {
        if (!rows.at(i).executionPathKey.isEmpty())
            index.insert(rows.at(i).executionPathKey, i);
    }
    return index;
}

int progressRowIndexForKey(
    const QHash<QString, int> &rowIndexByExecutionPathKey,
    const QString             &executionPathKey)
{
    const auto it =
        rowIndexByExecutionPathKey.constFind(executionPathKey);
    return it == rowIndexByExecutionPathKey.constEnd()
        ? -1
        : it.value();
}

void applyChildProgressSnapshot(
    QVector<PathProgressSnapshot>       *rows,
    const QHash<QString, int>           &rowIndexByExecutionPathKey,
    int                                  fallbackIndex,
    const ExecutionProgressSnapshot     &childSnapshot)
{
    if (!rows)
        return;

    bool applied = false;
    for (const auto &childRow : childSnapshot.paths)
    {
        const int rowIndex = progressRowIndexForKey(
            rowIndexByExecutionPathKey, childRow.executionPathKey);
        if (rowIndex < 0)
            continue;

        if (rowIndex < rows->size())
        {
            (*rows)[rowIndex] = childRow;
            applied = true;
        }
    }

    if (!applied
        && childSnapshot.paths.size() == 1
        && fallbackIndex >= 0
        && fallbackIndex < rows->size())
    {
        (*rows)[fallbackIndex] = childSnapshot.paths.first();
    }
}

ExecutionProgressSnapshot composeIsolatedProgressSnapshot(
    const QVector<PathProgressSnapshot> &rows,
    double                               aggregatePercent,
    int                                  activeAlternativeIndex,
    int                                  alternativeCount)
{
    ExecutionProgressSnapshot snapshot;
    snapshot.aggregatePercent =
        clampProgressPercent(aggregatePercent);
    snapshot.activeAlternativeIndex = activeAlternativeIndex;
    snapshot.alternativeCount = alternativeCount;
    snapshot.paths = rows;

    for (const auto &row : rows)
    {
        if (!row.executable)
            continue;

        ++snapshot.executablePathCount;
        snapshot.executableSegmentCount += row.totalSegments;
        snapshot.completedExecutableSegments +=
            std::min(row.completedSegments, row.totalSegments);

        if (row.lifecycle == PathLifecycleState::Completed)
            ++snapshot.completedExecutablePaths;
    }

    return snapshot;
}

ExecutionLedger isolatedLedgerFromProgressRows(
    const QVector<PathProgressSnapshot> &rows)
{
    ExecutionLedger ledger;

    for (const auto &row : rows)
    {
        if (row.executionPathKey.isEmpty())
            continue;

        PathExecutionState pathState;
        pathState.executionPathKey = row.executionPathKey;
        pathState.lifecycle = row.lifecycle;
        pathState.activeSegmentIndex = row.activeSegmentIndex;
        if (row.lifecycle == PathLifecycleState::Failed)
            pathState.lastError = row.message;
        ledger.pathStates.insert(row.executionPathKey, pathState);

        QVector<SegmentExecutionState> segmentStates;
        segmentStates.reserve(row.segments.size());
        for (const auto &segment : row.segments)
        {
            SegmentExecutionState segmentState;
            segmentState.executionPathKey = row.executionPathKey;
            segmentState.segmentIndex = segment.segmentIndex;
            segmentState.lifecycle = segment.lifecycle;
            segmentState.networkName = segment.networkName;
            segmentState.lastTerminalId = segment.endTerminalId;
            segmentStates.append(segmentState);
        }
        ledger.segmentStates.insert(row.executionPathKey,
                                    segmentStates);
    }

    return ledger;
}

bool hasPendingPostArrivalWork(
    const ScenarioExecutionPlan &plan,
    const ExecutionLedger       &ledger)
{
    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        const auto segmentStatesIt =
            ledger.segmentStates.constFind(pathPlan.executionPathKey);
        if (segmentStatesIt == ledger.segmentStates.constEnd())
            continue;

        for (const auto &segmentState : segmentStatesIt.value())
        {
            switch (segmentState.lifecycle)
            {
            case SegmentLifecycleState::VehicleArrived:
            case SegmentLifecycleState::UnloadCompleted:
            case SegmentLifecycleState::TerminalHandoffCompleted:
            case SegmentLifecycleState::TerminalProcessing:
            case SegmentLifecycleState::ReadyForPickup:
                return true;
            default:
                break;
            }
        }
    }

    return false;
}

QString validateExecutionPlan(const ScenarioExecutionPlan &plan)
{
    if (plan.executionId.isEmpty())
    {
        return QStringLiteral(
            "Execution plan is missing an execution ID");
    }

    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.executionPathKey.isEmpty())
        {
            return QStringLiteral(
                "Execution plan contains a path with an empty prepared identity");
        }

        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        if (!pathPlan.hasSequentialSegments())
        {
            return QStringLiteral(
                "Execution plan path %1 has non-sequential segments")
                .arg(pathPlan.executionPathKey);
        }

        if (pathPlan.segments.isEmpty())
        {
            return QStringLiteral(
                "Execution plan path %1 is executable but has no segments")
                .arg(pathPlan.executionPathKey);
        }

        for (const auto &segmentPlan : pathPlan.segments)
        {
            if (!segmentPlan.isWellFormed())
            {
                return QStringLiteral(
                    "Execution plan path %1 has a malformed segment at index %2")
                    .arg(pathPlan.executionPathKey)
                    .arg(segmentPlan.segmentIndex);
            }
        }

        if (pathPlan.transitions.size()
            != pathPlan.segments.size() - 1)
        {
            return QStringLiteral(
                "Execution plan path %1 has an inconsistent transition count")
                .arg(pathPlan.executionPathKey);
        }

        for (const auto &transition : pathPlan.transitions)
        {
            if (!transition.isSequential())
            {
                return QStringLiteral(
                    "Execution plan path %1 has a non-sequential transition from %2 to %3")
                    .arg(pathPlan.executionPathKey)
                    .arg(transition.fromSegmentIndex)
                    .arg(transition.toSegmentIndex);
            }
        }
    }

    return {};
}

ExecutableSelection collectExecutableSelection(
    const ScenarioExecutionPlan &plan,
    const QList<CargoNetSim::Backend::Path *> &selectedPaths,
    const QVector<QString> &selectedExecutionPathKeys,
    QString *err)
{
    ExecutableSelection selection;
    QHash<QString, CargoNetSim::Backend::Path *> pathByExecutionKey;
    pathByExecutionKey.reserve(selectedExecutionPathKeys.size());

    for (int i = 0; i < selectedExecutionPathKeys.size(); ++i)
    {
        pathByExecutionKey.insert(selectedExecutionPathKeys[i],
                                  selectedPaths.value(i));
    }

    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        const auto it =
            pathByExecutionKey.constFind(pathPlan.executionPathKey);
        if (it == pathByExecutionKey.constEnd() || !it.value())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Executable path key %1 was not found in the selected path set")
                           .arg(pathPlan.executionPathKey);
            }
            return {};
        }

        selection.executionPathKeys.append(pathPlan.executionPathKey);
        selection.paths.append(it.value());
    }

    if (err)
        err->clear();
    return selection;
}

void emitSupplementalPlanningStatus(
    ScenarioExecutor *executor,
    const ScenarioExecutionPlan &plan)
{
    const QString schedulingMessage =
        QStringLiteral("Execution plan: scheduling policy %1")
            .arg(schedulingPolicyLabel(plan.schedulingPolicy));
    qCInfo(lcScenario) << schedulingMessage;
    emit executor->statusMessage(schedulingMessage);

    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition == PlannedPathDisposition::Execute)
            continue;

        const QString message = QStringLiteral(
            "Execution plan: path %1 (%2) -> %3%4")
                                    .arg(pathPlan.executionPathKey,
                                         pathPlan.canonicalPathKey,
                                         dispositionLabel(pathPlan.disposition),
                                         pathPlan.planningMessage.isEmpty()
                                             ? QString()
                                             : QStringLiteral(": %1")
                                                   .arg(pathPlan.planningMessage));
        qCInfo(lcScenario) << message;
        emit executor->statusMessage(message);
    }
}

double resolveTimeStepSeconds(const ScenarioDocument &document,
                              ConfigController       *config)
{
    const auto timeStep = document.simulation.timeStepUnits();
    if (timeStep.has_value() && timeStep->value() > 0.0)
        return timeStep->value();

    if (config)
    {
        const double configValue =
            config->getSimulationParams()
                .value(QStringLiteral("time_step"),
                       kDefaultOrchestrationTimeStepSeconds)
                .toDouble();
        if (configValue > 0.0)
            return configValue;
    }

    return kDefaultOrchestrationTimeStepSeconds;
}

std::optional<double> resolveEndTimeSeconds(
    const ScenarioDocument &document)
{
    const auto endTime = document.simulation.endTimeUnits();
    return endTime.has_value() && endTime->value() > 0.0
        ? std::optional<double>(endTime->value())
        : std::nullopt;
}

bool hasExecutablePathFailure(const ScenarioExecutionPlan &plan,
                              const ExecutionLedger       &ledger,
                              QString                     *message)
{
    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        const auto it =
            ledger.pathStates.constFind(pathPlan.executionPathKey);
        if (it == ledger.pathStates.constEnd())
            continue;

        if (it.value().lifecycle == PathLifecycleState::Failed)
        {
            if (message)
            {
                *message = it.value().lastError.isEmpty()
                    ? QStringLiteral(
                          "Executable path %1 failed during live execution")
                          .arg(pathPlan.executionPathKey)
                    : it.value().lastError;
            }
            return true;
        }
    }

    if (message)
        message->clear();
    return false;
}

bool allExecutablePathsCompleted(const ScenarioExecutionPlan &plan,
                                 const ExecutionLedger       &ledger)
{
    bool sawExecutablePath = false;
    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        sawExecutablePath = true;
        const auto it =
            ledger.pathStates.constFind(pathPlan.executionPathKey);
        if (it == ledger.pathStates.constEnd())
            return false;

        if (it.value().lifecycle != PathLifecycleState::Completed)
            return false;
    }

    return sawExecutablePath;
}

QString joinQueueErrors(const QStringList &errors)
{
    return errors.join(QStringLiteral("; "));
}

} // namespace

ScenarioExecutor::ScenarioExecutor(QObject *parent)
    : QObject(parent)
{
}

void ScenarioExecutor::setDocument(const ScenarioDocument *document)
{
    m_document = document;
}

void ScenarioExecutor::setRegistry(const ScenarioRegistry *registry)
{
    m_registry = registry;
}

void ScenarioExecutor::setPaths(
    const QList<CargoNetSim::Backend::Path *> &paths)
{
    m_paths = paths;
}

void ScenarioExecutor::setExecutionPathKeys(
    const QVector<QString> &executionPathKeys)
{
    m_executionPathKeys = executionPathKeys;
}

void ScenarioExecutor::setDemandPolicy(
    ExecutionDemandPolicy demandPolicy)
{
    m_demandPolicy = demandPolicy;
}

void ScenarioExecutor::setIsolationPolicy(
    ExecutionIsolationPolicy isolationPolicy)
{
    m_isolationPolicy = isolationPolicy;
}

void ScenarioExecutor::requestStop()
{
    m_stopRequested.store(true);
}

void ScenarioExecutor::requestPause()
{
    m_pauseRequested.store(true);
}

void ScenarioExecutor::requestResume()
{
    m_pauseRequested.store(false);
}

bool ScenarioExecutor::validateInputs(QString *err)
{
    qCDebug(lcScenario) << "ScenarioExecutor::validateInputs: entry";
    if (!m_document)
    {
        if (err)
            *err = QStringLiteral("Scenario document not set");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: document not set";
        return false;
    }
    if (!m_registry)
    {
        if (err)
            *err = QStringLiteral("Scenario registry not set");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: registry not set";
        return false;
    }
    if (m_document->originContainers().isEmpty())
    {
        if (err)
            *err = QStringLiteral(
                "No containers in the origin terminal");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: no containers in origin";
        return false;
    }
    if (m_paths.isEmpty())
    {
        if (err)
            *err =
                QStringLiteral("No paths selected for simulation");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: no paths selected";
        return false;
    }
    qCDebug(lcScenario) << "ScenarioExecutor::validateInputs: passed, paths:" << m_paths.size();
    return true;
}

bool ScenarioExecutor::runIsolatedAlternativeExecutions()
{
    qCInfo(lcScenario)
        << "ScenarioExecutor::runIsolatedAlternativeExecutions:"
        << "starting"
        << "pathCount=" << m_paths.size()
        << "policy=" << isolationPolicyLabel(m_isolationPolicy);

    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();

    ScenarioExecutionResultSet aggregateResults;
    const int totalAlternatives = m_paths.size();
    double lastReportedTimeSeconds = 0.0;

    emit statusMessage(QStringLiteral(
        "Starting isolated what-if path execution (%1 alternative(s))")
                           .arg(totalAlternatives));

    auto failIsolatedRun = [&](const QString &message) -> bool {
        qCWarning(lcScenario)
            << "ScenarioExecutor::runIsolatedAlternativeExecutions:"
            << message;
        emit errorMessage(message);
        emit failed(message);
        emit finished();
        return false;
    };

    const auto progressAllocation =
        ContainerAllocator::allocate(*m_document, m_paths,
                                     m_demandPolicy);
    ExecutionPlanBuilder progressPlanBuilder(*m_document);
    const auto progressPlanResult = progressPlanBuilder.build(
        m_paths, m_executionPathKeys, progressAllocation,
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        m_demandPolicy);
    if (!progressPlanResult.isSuccess())
    {
        return failIsolatedRun(
            progressPlanResult.errorMessage.isEmpty()
                ? QStringLiteral(
                      "Failed to build aggregate isolated progress plan")
                : progressPlanResult.errorMessage);
    }

    m_executionPlan = progressPlanResult.plan;
    m_executionLedger = ExecutionLedger();
    const auto initialProgress =
        calculateExecutionProgress(m_executionPlan,
                                   m_executionLedger);
    QVector<PathProgressSnapshot> isolatedProgressRows =
        initialProgress.paths;
    m_executionLedger =
        isolatedLedgerFromProgressRows(isolatedProgressRows);
    const auto rowIndexByExecutionPathKey =
        indexProgressRowsByExecutionPathKey(isolatedProgressRows);

    auto aggregatePercentForAlternative =
        [totalAlternatives](int alternativeIndex,
                            double alternativePercent) {
        if (totalAlternatives <= 0)
            return 100.0;

        return ((static_cast<double>(alternativeIndex)
                 + (clampProgressPercent(alternativePercent)
                    / 100.0))
                / static_cast<double>(totalAlternatives))
               * 100.0;
    };

    emit progressSnapshotChanged(
        0.0,
        composeIsolatedProgressSnapshot(
            isolatedProgressRows, 0.0,
            totalAlternatives > 0 ? 0 : -1,
            totalAlternatives));
    emit progressChanged(0.0, 0.0);

    for (int index = 0; index < m_paths.size(); ++index)
    {
        if (m_stopRequested.load())
            return failIsolatedRun(QStringLiteral(
                "Simulation stopped by user request"));

        while (m_pauseRequested.load())
        {
            QThread::msleep(50);
            if (m_stopRequested.load())
                return failIsolatedRun(QStringLiteral(
                    "Simulation stopped by user request"));
        }

        auto *path = m_paths[index];
        const QString executionPathKey =
            index < m_executionPathKeys.size()
                ? m_executionPathKeys[index]
                : (path ? path->canonicalPathKey() : QString());
        if (!path || executionPathKey.isEmpty())
        {
            return failIsolatedRun(QStringLiteral(
                "Isolated what-if execution received an invalid path at index %1")
                                      .arg(index));
        }

        emit statusMessage(QStringLiteral(
            "Preparing isolated alternative %1/%2: %3")
                               .arg(index + 1)
                               .arg(totalAlternatives)
                               .arg(executionPathKey));

        QString err;
        if (!TerminalGraphBootstrap::resetAndLoad(
                *m_document, *m_registry, controller, &err,
                QStringLiteral(
                    "ScenarioExecutor::isolatedAlternative")))
        {
            return failIsolatedRun(err.isEmpty()
                                       ? QStringLiteral(
                                             "Failed to reset TerminalSim for isolated what-if path")
                                       : err);
        }

        if (!resetModeStateForAlternative(controller, path, &err))
        {
            return failIsolatedRun(err.isEmpty()
                                       ? QStringLiteral(
                                             "Failed to reset simulator state for isolated what-if path")
                                       : err);
        }

        ScenarioExecutor childExecutor;
        childExecutor.setDocument(m_document);
        childExecutor.setRegistry(m_registry);
        childExecutor.setPaths({path});
        childExecutor.setExecutionPathKeys({executionPathKey});
        childExecutor.setDemandPolicy(m_demandPolicy);
        childExecutor.setIsolationPolicy(
            ExecutionIsolationPolicy::SharedSimulatorState);

        QString childError;
        connect(&childExecutor,
                &ScenarioExecutor::statusMessage,
                this,
                [this, index, totalAlternatives](const QString &message) {
                    emit statusMessage(QStringLiteral(
                        "Alternative %1/%2: %3")
                                           .arg(index + 1)
                                           .arg(totalAlternatives)
                                           .arg(message));
                });
        connect(&childExecutor,
                &ScenarioExecutor::errorMessage,
                this,
                [&childError](const QString &message) {
                    childError = message;
                });
        connect(&childExecutor,
                &ScenarioExecutor::progressChanged,
                this,
                [this, index, totalAlternatives,
                 &lastReportedTimeSeconds,
                 aggregatePercentForAlternative](
                    double currentTime,
                    double percent) {
                    lastReportedTimeSeconds = currentTime;
                    emit progressChanged(currentTime,
                                         aggregatePercentForAlternative(
                                             index, percent));
                });
        connect(&childExecutor,
                &ScenarioExecutor::progressSnapshotChanged,
                this,
                [this, index, totalAlternatives,
                 &isolatedProgressRows,
                 &rowIndexByExecutionPathKey,
                aggregatePercentForAlternative](
                    double currentTime,
                    const ExecutionProgressSnapshot &snapshot) {
                    applyChildProgressSnapshot(
                        &isolatedProgressRows,
                        rowIndexByExecutionPathKey, index,
                        snapshot);
                    m_executionLedger =
                        isolatedLedgerFromProgressRows(
                            isolatedProgressRows);
                    emit progressSnapshotChanged(currentTime,
                        composeIsolatedProgressSnapshot(
                            isolatedProgressRows,
                            aggregatePercentForAlternative(
                                index,
                                snapshot.aggregatePercent),
                            index, totalAlternatives));
                });

        if (!childExecutor.run())
        {
            return failIsolatedRun(childError.isEmpty()
                                       ? QStringLiteral(
                                             "Isolated what-if alternative failed: %1")
                                             .arg(executionPathKey)
                                       : childError);
        }

        for (const auto &pathResult :
             childExecutor.executionResults().pathResults())
        {
            aggregateResults.addPathResult(pathResult);
        }

        m_dispatchableSegments =
            childExecutor.dispatchableSegments();

        const int completedRowIndex =
            progressRowIndexForKey(rowIndexByExecutionPathKey,
                                   executionPathKey);
        if (completedRowIndex >= 0
            && completedRowIndex < isolatedProgressRows.size())
        {
            markProgressRowCompleted(
                &isolatedProgressRows[completedRowIndex]);
        }
        m_executionLedger =
            isolatedLedgerFromProgressRows(isolatedProgressRows);
        emit progressSnapshotChanged(
            lastReportedTimeSeconds,
            composeIsolatedProgressSnapshot(
                isolatedProgressRows,
                aggregatePercentForAlternative(index, 100.0),
                index, totalAlternatives));
        emit progressChanged(
            lastReportedTimeSeconds,
            aggregatePercentForAlternative(index, 100.0));

        emit statusMessage(QStringLiteral(
            "Completed isolated alternative %1/%2: %3")
                               .arg(index + 1)
                               .arg(totalAlternatives)
                               .arg(executionPathKey));
    }

    m_executionResults = aggregateResults;
    m_executionLedger =
        isolatedLedgerFromProgressRows(isolatedProgressRows);
    emit progressSnapshotChanged(
        lastReportedTimeSeconds,
        composeIsolatedProgressSnapshot(
            isolatedProgressRows, 100.0,
            totalAlternatives > 0 ? totalAlternatives - 1 : -1,
            totalAlternatives));
    emit progressChanged(lastReportedTimeSeconds, 100.0);
    emit statusMessage(QStringLiteral(
        "Isolated what-if validation completed successfully"));
    emit succeeded();
    emit finished();
    return true;
}

bool ScenarioExecutor::run()
{
    qCInfo(lcScenario) << "ScenarioExecutor::run: started";
    m_executionResults = ScenarioExecutionResultSet();
    m_executionPlan    = ScenarioExecutionPlan();
    m_executionLedger  = ExecutionLedger();
    m_dispatchableSegments.clear();
    m_stopRequested.store(false);
    m_pauseRequested.store(false);

    QString err;
    try
    {
        if (!validateInputs(&err))
        {
            qCWarning(lcScenario) << "ScenarioExecutor::run: validation failed:" << err;
            emit errorMessage(err);
            emit failed(err);
            emit finished();
            return false;
        }

        if (m_isolationPolicy
            == ExecutionIsolationPolicy::IsolatedAlternatives)
        {
            return runIsolatedAlternativeExecutions();
        }

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        auto *config     = controller.getConfigController();
        auto *vehicles   = controller.getVehicleController();
        auto *regionData = controller.getRegionDataController();
        auto *terminalClient = controller.getTerminalClient();
        TerminalSimulationInventoryGateway terminalInventoryGateway(
            terminalClient);
        TerminalPickupCoordinator terminalPickupCoordinator(
            &terminalInventoryGateway);
        TerminalPickupCoordinator *pickupCoordinator =
            terminalClient ? &terminalPickupCoordinator : nullptr;
        const QString executionId =
            QUuid::createUuid().toString(QUuid::WithoutBraces);

        const auto allocation =
            ContainerAllocator::allocate(*m_document, m_paths,
                                         m_demandPolicy);
        qCInfo(lcScenario)
            << "ScenarioExecutor::run:"
            << "container allocation completed"
            << "selectedPaths=" << m_paths.size()
            << "selectedExecutionPathKeyCount=" << m_executionPathKeys.size()
            << "executionId=" << executionId;
        for (auto *path : m_paths)
        {
            if (!path)
                continue;
            qCDebug(lcScenario)
                << "ScenarioExecutor::run: allocation summary"
                << "pathId=" << path->getPathId()
                << "pathKey=" << path->canonicalPathKey()
                << "rank=" << path->getRank()
                << "effectiveContainerCount="
                << allocation.effectiveContainerCountForPath(path)
                << "allocatedContainers="
                << allocation.containersForPath(path).size()
                << "segmentCount="
                << path->getSegments().size();
        }

        ExecutionPlanBuilder executionPlanBuilder(*m_document);
        const auto planResult = executionPlanBuilder.build(
            m_paths, m_executionPathKeys, allocation, executionId,
            m_demandPolicy);
        if (!planResult.isSuccess())
        {
            const QString message =
                planResult.errorMessage.isEmpty()
                    ? QStringLiteral("Failed to build execution plan")
                    : planResult.errorMessage;
            qCWarning(lcScenario)
                << "ScenarioExecutor::run: execution plan build failed:"
                << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        }

        m_executionPlan = planResult.plan;
        const QString planValidationError =
            validateExecutionPlan(m_executionPlan);
        if (!planValidationError.isEmpty())
        {
            qCWarning(lcScenario)
                << "ScenarioExecutor::run: execution plan validation failed:"
                << planValidationError;
            emit errorMessage(planValidationError);
            emit failed(planValidationError);
            emit finished();
            return false;
        }

        qCInfo(lcScenario)
            << "ScenarioExecutor::run: execution plan built"
            << "selectedPaths=" << m_executionPlan.paths.size()
            << "executablePaths=" << m_executionPlan.executablePathCount()
            << "demandPolicy="
            << static_cast<int>(m_executionPlan.demandPolicy)
            << "schedulingPolicy="
            << schedulingPolicyLabel(m_executionPlan.schedulingPolicy);
        emitSupplementalPlanningStatus(this, m_executionPlan);

        PathExecutionCoordinator coordinator;
        if (!coordinator.initialize(m_executionPlan, &err))
        {
            const QString message =
                err.isEmpty()
                    ? QStringLiteral(
                          "Failed to initialize path execution coordinator")
                    : err;
            qCWarning(lcScenario)
                << "ScenarioExecutor::run: coordinator initialization failed:"
                << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        }

        const auto coordinatorSnapshot = coordinator.snapshot();
        m_executionLedger = coordinatorSnapshot.ledger;
        m_dispatchableSegments =
            coordinatorSnapshot.dispatchableSegments;

        if (m_executionPlan.executablePathCount() > 0
            && m_dispatchableSegments.isEmpty())
        {
            const QString message = QStringLiteral(
                "Execution plan has executable paths but no initial dispatchable segments");
            qCWarning(lcScenario)
                << "ScenarioExecutor::run:" << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        }

        qCInfo(lcScenario)
            << "ScenarioExecutor::run: coordinator initialized"
            << "dispatchableSegments=" << m_dispatchableSegments.size();

        const double deltaTSeconds =
            resolveTimeStepSeconds(*m_document, config);
        const auto endTimeSeconds =
            resolveEndTimeSeconds(*m_document);

        if (m_executionPlan.executablePathCount() > 0)
        {
            if (terminalClient
                && !terminalClient->resetRuntimeState())
            {
                const QString message = QStringLiteral(
                    "Failed to reset TerminalSim runtime state before execution");
                qCWarning(lcScenario)
                    << "ScenarioExecutor::run:" << message;
                emit errorMessage(message);
                emit failed(message);
                emit finished();
                return false;
            }

            if (!coordinator.seedContainers(allocation, &err))
            {
                const QString message =
                    err.isEmpty()
                        ? QStringLiteral(
                              "Failed to seed execution container custody")
                        : err;
                qCWarning(lcScenario)
                    << "ScenarioExecutor::run: container seeding failed:"
                    << message;
                emit errorMessage(message);
                emit failed(message);
                emit finished();
                return false;
            }
            const auto seededSnapshot = coordinator.snapshot();
            m_executionLedger = seededSnapshot.ledger;
            m_dispatchableSegments =
                seededSnapshot.dispatchableSegments;

            qCInfo(lcScenario)
                << "ScenarioExecutor::run: execution custody seeded"
                << "dispatchableSegments="
                << m_dispatchableSegments.size();

            if (pickupCoordinator
                && !pickupCoordinator->seedExecutionInventory(
                    m_executionPlan, allocation,
                    /*addTimeSeconds=*/0.0, &err))
            {
                const QString message =
                    err.isEmpty()
                        ? QStringLiteral(
                              "Failed to seed terminal execution inventory")
                        : err;
                qCWarning(lcScenario)
                    << "ScenarioExecutor::run: terminal inventory seed failed:"
                    << message;
                emit errorMessage(message);
                emit failed(message);
                emit finished();
                return false;
            }

            if (terminalClient
                && !terminalClient->updateAllTerminalsSystemDynamics(
                    0.0,
                    deltaTSeconds))
            {
                const QString message = QStringLiteral(
                    "Failed to initialize TerminalSim system dynamics at CargoNetSim time zero");
                qCWarning(lcScenario)
                    << "ScenarioExecutor::run:" << message;
                emit errorMessage(message);
                emit failed(message);
                emit finished();
                return false;
            }
        }

        const auto executableSelection = collectExecutableSelection(
            m_executionPlan, m_paths, m_executionPathKeys, &err);
        if (!err.isEmpty())
        {
            qCWarning(lcScenario)
                << "ScenarioExecutor::run: executable selection failed:"
                << err;
            emit errorMessage(err);
            emit failed(err);
            emit finished();
            return false;
        }

        if (executableSelection.paths.isEmpty())
        {
            const QString message = QStringLiteral(
                "No executable paths under current execution policy");
            qCWarning(lcScenario)
                << "ScenarioExecutor::run:" << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        }

        BufferedExecutionEventQueue eventQueue;
        ExecutionEventAdapter eventAdapter;
        if (!eventAdapter.initialize(m_executionPlan, &err))
        {
            const QString message =
                err.isEmpty()
                    ? QStringLiteral(
                          "Failed to initialize live execution event adapter")
                    : err;
            qCWarning(lcScenario)
                << "ScenarioExecutor::run:" << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        }

        connect(&eventAdapter,
                &ExecutionEventAdapter::executionEventReady,
                this,
                [&eventQueue](const ExecutionEvent &event) {
                    eventQueue.pushEvent(event);
                },
                Qt::DirectConnection);
        connect(&eventAdapter, &ExecutionEventAdapter::adapterError,
                this,
                [&eventQueue](const QString &message) {
                    eventQueue.pushError(message);
                },
                Qt::DirectConnection);
        eventAdapter.attach(controller.getTrainClient(),
                            controller.getShipClient(),
                            controller.getTruckManager());

        DispatchableWaveBuilder waveBuilder(
            *m_registry, config, vehicles, executionId,
            pickupCoordinator);
        NetworkExecutionSessionManager sessionManager(
            *m_registry, regionData, controller.getTrainClient(),
            controller.getShipClient(), controller.getTruckManager(),
            controller.truckExecutablePath());

        double currentTimeSeconds = 0.0;
        int waveCounter = 0;
        LiveExecutionTimeOverrideGuard liveClockGuard{
            controller.getTrainClient(),
            controller.getShipClient()};
        liveClockGuard.set(currentTimeSeconds);
        const QVariantMap progressCostWeights =
            config ? config->getCostFunctionWeights() : QVariantMap{};
        const QVariantMap progressTransportModes =
            config ? config->getTransportModes() : QVariantMap{};

        auto refreshSnapshot = [&]() {
            const auto snapshot = coordinator.snapshot();
            m_executionLedger = snapshot.ledger;
            m_dispatchableSegments =
                snapshot.dispatchableSegments;
        };

        auto enrichProgressWithActuals =
            [&](ExecutionProgressSnapshot &snapshot) {
            for (auto &pathSnapshot : snapshot.paths)
            {
                if (!pathSnapshot.executable)
                    continue;

                int selectedIndex = -1;
                for (int i = 0;
                     i < executableSelection.executionPathKeys.size();
                     ++i)
                {
                    if (executableSelection.executionPathKeys[i]
                        == pathSnapshot.executionPathKey)
                    {
                        selectedIndex = i;
                        break;
                    }
                }

                if (selectedIndex < 0
                    || selectedIndex >= executableSelection.paths.size())
                {
                    continue;
                }

                auto *path = executableSelection.paths[selectedIndex];
                if (!path)
                    continue;

                const int containerCount =
                    allocation.effectiveContainerCountForPath(path);
                if (containerCount <= 0)
                    continue;

                const auto liveResult =
                    SegmentCostMath::computePathExecutionResult(
                        controller.getShipClient(),
                        controller.getTrainClient(),
                        controller.getTruckManager(),
                        path,
                        pathSnapshot.executionPathKey,
                        progressCostWeights,
                        progressTransportModes,
                        containerCount,
                        /*emitInfoLog=*/false);
                const auto actualMetrics =
                    liveResult.totalActualMetrics();
                const auto actualCosts =
                    liveResult.totalActualCosts();

                pathSnapshot.actualMetricsAvailable =
                    actualMetrics.available;
                pathSnapshot.actualCostsAvailable =
                    actualCosts.available;
                if (actualMetrics.available)
                {
                    pathSnapshot.actualDistanceKm =
                        Units::toKilometers(
                            actualMetrics.distanceUnits())
                            .value();
                    pathSnapshot.actualTravelTimeHours =
                        Units::toHours(
                            actualMetrics.travelTimeUnits())
                            .value();
                }
                if (actualCosts.available)
                {
                    pathSnapshot.actualEdgeCostUsd =
                        actualCosts.total();
                    pathSnapshot.actualTotalCostUsd =
                        pathSnapshot.actualEdgeCostUsd
                        + pathSnapshot.actualTerminalCostUsd;
                }
            }
        };

        auto emitExecutionProgress = [&](double timeSeconds) {
            auto progressSnapshot =
                calculateExecutionProgress(m_executionPlan,
                                           m_executionLedger);
            enrichProgressWithActuals(progressSnapshot);
            emit progressSnapshotChanged(timeSeconds,
                                         progressSnapshot);
            emit progressChanged(
                timeSeconds,
                progressSnapshot.aggregatePercent);
        };

        emit statusMessage(QStringLiteral(
            "Starting multimodal path execution"));
        emitExecutionProgress(currentTimeSeconds);

        auto failRun = [&](const QString &message) -> bool {
            qCWarning(lcScenario)
                << "ScenarioExecutor::run:" << message;
            emit errorMessage(message);
            emit failed(message);
            emit finished();
            return false;
        };

        auto applyQueuedEvents = [&]() -> bool {
            const QStringList queueErrors =
                eventQueue.drainErrors();
            if (!queueErrors.isEmpty())
            {
                err = joinQueueErrors(queueErrors);
                return false;
            }

            const auto queuedEvents = eventQueue.drainEvents();
            for (const auto &event : queuedEvents)
            {
                const auto outcome =
                    coordinator.applyEvent(event);
                if (!outcome.accepted
                    && !outcome.errorMessage.isEmpty())
                {
                    err = outcome.errorMessage;
                    return false;
                }

                if (outcome.accepted)
                    sessionManager.observeExecutionEvent(event);
            }

            refreshSnapshot();
            return true;
        };

        auto awaitPostArrivalControlPlaneProgress =
            [&]() -> bool {
            if (!hasPendingPostArrivalWork(m_executionPlan,
                                           m_executionLedger))
            {
                return true;
            }

            emit statusMessage(QStringLiteral(
                "Awaiting post-arrival unload and terminal handoff events"));

            QElapsedTimer waitTimer;
            waitTimer.start();

            while (waitTimer.elapsed() < kPostArrivalEventWaitMs)
            {
                const int remainingMs = qMax(
                    1, kPostArrivalEventWaitMs
                           - static_cast<int>(waitTimer.elapsed()));
                if (!eventQueue.waitForWork(remainingMs))
                    break;

                if (!applyQueuedEvents())
                    return false;

                refreshSnapshot();
                emitExecutionProgress(currentTimeSeconds);

                QString failureMessage;
                if (hasExecutablePathFailure(m_executionPlan,
                                             m_executionLedger,
                                             &failureMessage))
                {
                    err = failureMessage;
                    return false;
                }

                if (allExecutablePathsCompleted(m_executionPlan,
                                                m_executionLedger))
                {
                    return true;
                }

                if (sessionManager.hasActiveVehicles()
                    || !m_dispatchableSegments.isEmpty()
                    || !hasPendingPostArrivalWork(
                        m_executionPlan, m_executionLedger))
                {
                    return true;
                }
            }

            err = QStringLiteral(
                "Live execution stalled while awaiting post-arrival unload or terminal handoff completion");
            return false;
        };

        auto advanceTerminalOnly = [&]() -> bool {
            const double nextTimeSeconds =
                currentTimeSeconds + deltaTSeconds;
            liveClockGuard.set(nextTimeSeconds);

            if (!terminalClient
                || !terminalClient->updateAllTerminalsSystemDynamics(
                    nextTimeSeconds,
                    deltaTSeconds))
            {
                err = QStringLiteral(
                    "Failed to advance terminal system dynamics while waiting for terminal pickup readiness");
                return false;
            }

            currentTimeSeconds = nextTimeSeconds;
            emitExecutionProgress(currentTimeSeconds);

            if (!applyQueuedEvents())
                return false;

            refreshSnapshot();
            if (endTimeSeconds.has_value()
                && currentTimeSeconds >= endTimeSeconds.value())
            {
                err = QStringLiteral(
                    "Simulation reached the configured end time while waiting for terminal pickup readiness");
                return false;
            }

            return true;
        };

        while (true)
        {
            if (m_stopRequested.load())
            {
                err = QStringLiteral(
                    "Simulation stopped by user request");
                return failRun(err);
            }

            while (m_pauseRequested.load())
            {
                QThread::msleep(50);
                if (m_stopRequested.load())
                {
                    err = QStringLiteral(
                        "Simulation stopped by user request");
                    return failRun(err);
                }
            }

            if (!applyQueuedEvents())
                return failRun(err);

            refreshSnapshot();
            QString failureMessage;
            if (hasExecutablePathFailure(m_executionPlan,
                                         m_executionLedger,
                                         &failureMessage))
            {
                return failRun(failureMessage);
            }

            if (allExecutablePathsCompleted(m_executionPlan,
                                            m_executionLedger))
            {
                emitExecutionProgress(currentTimeSeconds);
                break;
            }

            if (!m_dispatchableSegments.isEmpty())
            {
                const auto wave = waveBuilder.build(
                    m_executionPlan, m_executionLedger,
                    m_dispatchableSegments);
                if (wave.terminalBlocked)
                {
                    emit statusMessage(
                        wave.terminalBlockedMessage.isEmpty()
                            ? QStringLiteral(
                                  "Waiting for terminal pickup readiness")
                            : wave.terminalBlockedMessage);
                    if (!advanceTerminalOnly())
                        return failRun(err);
                    continue;
                }
                if (!wave.isSuccess())
                {
                    err = wave.errorMessage.isEmpty()
                        ? QStringLiteral(
                              "Failed to build dispatchable execution wave")
                        : wave.errorMessage;
                    return failRun(err);
                }

                if (wave.assignments.isEmpty())
                {
                    err = QStringLiteral(
                        "Dispatchable execution wave produced no vehicle assignments");
                    return failRun(err);
                }

                ++waveCounter;
                emit statusMessage(
                    QStringLiteral(
                        "Dispatching execution wave %1 (%2 segment(s), %3 vehicle(s))")
                        .arg(waveCounter)
                        .arg(m_dispatchableSegments.size())
                        .arg(wave.assignments.size()));

                if (!sessionManager.dispatchWave(
                        m_executionPlan, wave.bundle,
                        wave.assignments, &err))
                {
                    if (pickupCoordinator)
                    {
                        QString releaseErr;
                        if (!pickupCoordinator->releaseReservations(
                                wave.pickupReservations,
                                &releaseErr))
                        {
                            qCWarning(lcScenario)
                                << "ScenarioExecutor::run:"
                                << "failed to release terminal pickup reservations after dispatch failure:"
                                << releaseErr;
                        }
                    }
                    err = err.isEmpty()
                        ? QStringLiteral(
                              "Failed to dispatch live execution wave")
                        : err;
                    return failRun(err);
                }

                if (pickupCoordinator
                    && !pickupCoordinator->commitReservations(
                        wave.pickupReservations,
                        currentTimeSeconds,
                        &err))
                {
                    err = err.isEmpty()
                        ? QStringLiteral(
                              "Failed to commit terminal pickup reservations")
                        : err;
                    return failRun(err);
                }

                if (!coordinator.registerDispatchAssignments(
                        wave.assignments, &err))
                {
                    err = err.isEmpty()
                        ? QStringLiteral(
                              "Failed to register live dispatch assignments")
                        : err;
                    return failRun(err);
                }

                for (const auto &assignment : wave.assignments)
                {
                    ExecutionEvent dispatchEvent;
                    if (!eventAdapter.buildDispatchEvent(
                            assignment.vehicleId,
                            assignment.networkName,
                            currentTimeSeconds, &dispatchEvent,
                            &err))
                    {
                        err = err.isEmpty()
                            ? QStringLiteral(
                                  "Failed to build normalized dispatch event")
                            : err;
                        return failRun(err);
                    }

                    const auto outcome =
                        coordinator.applyEvent(dispatchEvent);
                    if (!outcome.accepted
                        && !outcome.errorMessage.isEmpty())
                    {
                        err = outcome.errorMessage;
                        return failRun(err);
                    }
                }

                refreshSnapshot();
                emitExecutionProgress(currentTimeSeconds);
            }

            if (!sessionManager.hasActiveVehicles())
            {
                refreshSnapshot();
                if (allExecutablePathsCompleted(m_executionPlan,
                                                m_executionLedger))
                {
                    emitExecutionProgress(currentTimeSeconds);
                    break;
                }

                if (m_dispatchableSegments.isEmpty())
                {
                    if (!hasPendingPostArrivalWork(
                            m_executionPlan, m_executionLedger))
                    {
                        err = QStringLiteral(
                            "Live execution stalled: no active vehicles and no dispatchable segments remain");
                        return failRun(err);
                    }

                    if (!awaitPostArrivalControlPlaneProgress())
                        return failRun(err);

                    continue;
                }

                continue;
            }

            const double nextTimeSeconds =
                currentTimeSeconds + deltaTSeconds;
            liveClockGuard.set(nextTimeSeconds);

            if (!sessionManager.advanceActiveSessions(deltaTSeconds,
                                                      &err))
            {
                err = err.isEmpty()
                    ? QStringLiteral(
                          "Failed to advance live execution sessions")
                    : err;
                return failRun(err);
            }

            if (terminalClient
                && !terminalClient->updateAllTerminalsSystemDynamics(
                    nextTimeSeconds,
                    deltaTSeconds))
            {
                err = QStringLiteral(
                    "Failed to advance terminal system dynamics");
                return failRun(err);
            }

            if (!applyQueuedEvents())
                return failRun(err);

            currentTimeSeconds = nextTimeSeconds;
            emitExecutionProgress(currentTimeSeconds);

            refreshSnapshot();
            if (hasExecutablePathFailure(m_executionPlan,
                                         m_executionLedger,
                                         &failureMessage))
            {
                return failRun(failureMessage);
            }

            if (allExecutablePathsCompleted(m_executionPlan,
                                            m_executionLedger))
            {
                emitExecutionProgress(currentTimeSeconds);
                break;
            }

            if (endTimeSeconds.has_value()
                && currentTimeSeconds >= endTimeSeconds.value())
            {
                if (!sessionManager.hasActiveVehicles()
                    && m_dispatchableSegments.isEmpty()
                    && hasPendingPostArrivalWork(
                        m_executionPlan, m_executionLedger))
                {
                    continue;
                }

                err = QStringLiteral(
                    "Simulation reached the configured end time before all executable paths completed");
                return failRun(err);
            }
        }

        // Extract per-path results from simulator state.
        qCDebug(lcScenario) << "ScenarioExecutor::run: extracting results";
        ResultsExtractor extractor(controller.getShipClient(),
                                   controller.getTrainClient(),
                                   controller.getTruckManager(),
                                   terminalClient,
                                   config, this);
        connect(&extractor, &ResultsExtractor::statusMessage,
                this, &ScenarioExecutor::statusMessage);
        connect(&extractor, &ResultsExtractor::errorMessage,
                this, &ScenarioExecutor::errorMessage);

        m_executionResults = extractor.extractExecutionResults(
            executableSelection.paths,
            executableSelection.executionPathKeys,
            &allocation,
            executionId);
        qCDebug(lcScenario) << "ScenarioExecutor::run: extracted"
                            << m_executionResults.size() << "path results";

        emit statusMessage(QStringLiteral(
            "Simulation validation completed successfully"));
        emit succeeded();
    }
    catch (const std::exception &e)
    {
        qCCritical(lcScenario) << "ScenarioExecutor::run: exception:" << e.what();
        const QString message =
            QString("Error during simulation: %1").arg(e.what());
        emit errorMessage(message);
        emit failed(message);
        emit finished();
        return false;
    }
    catch (...)
    {
        qCCritical(lcScenario) << "ScenarioExecutor::run: unknown exception";
        const QString message =
            QStringLiteral("Unknown error during simulation");
        emit errorMessage(message);
        emit failed(message);
        emit finished();
        return false;
    }

    qCInfo(lcScenario) << "ScenarioExecutor::run: completed successfully";
    emit finished();
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
