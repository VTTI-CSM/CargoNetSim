#include "ScenarioRuntime.h"

#include <QMetaObject>
#include <QSet>
#include <QStringList>
#include <QThread>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "PreparedPathEligibilityService.h"
#include "ScenarioApplier.h"
#include "ScenarioDocument.h"
#include "ScenarioExecutor.h"
#include "ScenarioValidator.h"
#include "ValidationIssue.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ScenarioRuntime::ScenarioRuntime(
    std::unique_ptr<ScenarioDocument> doc, QObject *parent)
    : QObject(parent)
    , m_document(std::move(doc))
{
    // Relay CargoNetSimController progress/completion signals through
    // this runtime so consumers subscribe to one source of truth.
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    connect(&controller,
            &CargoNetSim::CargoNetSimController::
                simulationStepCompleted,
            this, &ScenarioRuntime::onStepCompleted);
}

ScenarioRuntime::~ScenarioRuntime()
{
    cleanupWorker();
}

bool ScenarioRuntime::load()
{
    qCInfo(lcScenario) << "ScenarioRuntime::load: entry";
    if (!m_document)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::load: no document to load";
        emit failed(QStringLiteral("No document to load"));
        return false;
    }

    // Plan-deviation: ScenarioValidator::validate and
    // ScenarioApplier::apply are static — call directly, no instance.
    qCDebug(lcScenario) << "ScenarioRuntime::load: validating document";
    const auto issues = ScenarioValidator::validate(*m_document);
    qCDebug(lcScenario) << "ScenarioRuntime::load: validation returned" << issues.size() << "issues";
    for (const auto &iss : issues)
    {
        if (iss.severity == ValidationIssue::Error)
        {
            qCWarning(lcScenario) << "ScenarioRuntime::load: validation error:" << iss.message;
            emit failed(QString("Validation error: %1")
                            .arg(iss.message));
            return false;
        }
    }

    qCDebug(lcScenario) << "ScenarioRuntime::load: applying scenario to controller";
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    QString err;
    if (!ScenarioApplier::apply(*m_document, controller,
                                m_registry, &err))
    {
        qCWarning(lcScenario) << "ScenarioRuntime::load: apply failed:" << err;
        emit failed(QString("Apply failed: %1").arg(err));
        return false;
    }

    m_endTime = m_document->simulation.endTimeUnits()
                    .value_or(Units::seconds(86400.0))
                    .value();
    controller.setSimulationEndTime(m_endTime);
    m_preparedPaths = PreparedPathSet();
    m_preparedPathEligibility.clear();
    m_paths.clear();
    m_selectedPathKeys.clear();
    m_lastExecutionResults = ScenarioExecutionResultSet();
    m_loaded = true;
    qCInfo(lcScenario) << "ScenarioRuntime::load: succeeded, endTime:" << m_endTime;
    return true;
}

void ScenarioRuntime::setPreparedPaths(
    const PreparedPathSet &preparedPaths)
{
    m_preparedPaths = preparedPaths;
    refreshPreparedPathEligibility();
    m_paths.clear();
    m_selectedPathKeys.clear();
    m_lastExecutionResults = ScenarioExecutionResultSet();
    qCDebug(lcScenario)
        << "ScenarioRuntime::setPreparedPaths: prepared"
        << m_preparedPaths.size() << "path(s)";
}

void ScenarioRuntime::refreshPreparedPathEligibility()
{
    if (m_preparedPaths.isEmpty())
    {
        m_preparedPathEligibility.clear();
        return;
    }

    m_preparedPathEligibility =
        PreparedPathEligibilityService::evaluateAll(
            m_preparedPaths,
            PreparedPathEligibilityService::currentAvailability());
}

bool ScenarioRuntime::setSelectedPathKeys(
    const QVector<QString> &pathKeys, QString *err)
{
    if (pathKeys.isEmpty())
    {
        m_selectedPathKeys.clear();
        m_paths.clear();
        qCDebug(lcScenario)
            << "ScenarioRuntime::setSelectedPathKeys: cleared selection";
        return true;
    }

    if (m_preparedPaths.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                "No prepared paths are available for selection");
        }
        return false;
    }

    QVector<QString> normalizedKeys;
    normalizedKeys.reserve(pathKeys.size());
    QSet<QString> seen;
    QStringList   missing;
    for (const auto &pathKey : pathKeys)
    {
        if (pathKey.isEmpty() || seen.contains(pathKey))
            continue;
        if (!m_preparedPaths.containsPathIdentity(pathKey))
        {
            missing.append(pathKey);
            continue;
        }
        seen.insert(pathKey);
        normalizedKeys.append(pathKey);
    }

    if (!missing.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                       "Unknown prepared path identity: %1")
                       .arg(missing.join(QStringLiteral(", ")));
        }
        return false;
    }

    m_selectedPathKeys = normalizedKeys;
    m_paths = m_preparedPaths.rawPaths(m_selectedPathKeys);
    qCDebug(lcScenario)
        << "ScenarioRuntime::setSelectedPathKeys: selected"
        << m_paths.size() << "path(s)";
    return true;
}

bool ScenarioRuntime::startSimulation()
{
    qCInfo(lcScenario) << "ScenarioRuntime::startSimulation: entry";
    if (!m_loaded)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::startSimulation: runtime not loaded";
        emit failed(QStringLiteral(
            "Runtime not loaded; call load() first"));
        return false;
    }
    if (m_workerThread)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::startSimulation: already running";
        emit failed(
            QStringLiteral("Simulation already running"));
        return false;
    }

    m_workerThread = new QThread(this);
    m_executor     = new ScenarioExecutor();
    m_terminalOutcome  = TerminalOutcome::None;
    m_failureMessage.clear();
    m_terminalSignaled = false;
    m_lastExecutionResults = ScenarioExecutionResultSet();

    // Plan-deviation: Task 23 decoupled the executor from the runtime.
    // Inject inputs directly instead of setContext(this).
    m_executor->setDocument(m_document.get());
    m_executor->setRegistry(&m_registry);
    m_executor->setPaths(m_paths);
    m_executor->setPathIdentities(m_selectedPathKeys);

    m_executor->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_executor,
            [this] { m_executor->run(); });
    connect(m_executor, &ScenarioExecutor::statusMessage,
            this, &ScenarioRuntime::statusMessage);
    connect(m_executor, &ScenarioExecutor::errorMessage,
            this, &ScenarioRuntime::errorMessage);
    connect(m_executor, &ScenarioExecutor::succeeded, this,
            &ScenarioRuntime::onExecutorSucceeded);
    connect(m_executor, &ScenarioExecutor::failed, this,
            &ScenarioRuntime::onExecutorFailed);
    connect(m_executor, &ScenarioExecutor::finished, this,
            &ScenarioRuntime::onExecutorFinished);

    m_workerThread->start();
    return true;
}

bool ScenarioRuntime::validateCurrentSelectionForSimulation(
    QString *err) const
{
    return PreparedPathEligibilityService::validateSelection(
        m_preparedPaths, m_selectedPathKeys,
        PreparedPathEligibilityService::currentAvailability(),
        err);
}

void ScenarioRuntime::onStepCompleted(double currentTime,
                                      double progress)
{
    // Log only on significant progress jumps (>=5%) to avoid spam
    if (progress - m_lastProgress >= 5.0 || progress >= 100.0)
        qCDebug(lcScenario) << "ScenarioRuntime::onStepCompleted: time:" << currentTime
                            << "progress:" << progress << "%";
    m_lastTime     = currentTime;
    m_lastProgress = progress;
    emit progressChanged(currentTime, progress);
}

void ScenarioRuntime::onExecutorFinished()
{
    qCInfo(lcScenario) << "ScenarioRuntime::onExecutorFinished: executor done";
    if (m_executor)
        m_lastExecutionResults = m_executor->executionResults();
    qCDebug(lcScenario) << "ScenarioRuntime::onExecutorFinished: collected"
                        << m_lastExecutionResults.size() << "results";
    cleanupWorker();

    switch (m_terminalOutcome)
    {
    case TerminalOutcome::Succeeded:
        if (!m_terminalSignaled)
        {
            m_terminalSignaled = true;
            emit completed();
        }
        break;
    case TerminalOutcome::Failed:
        if (!m_terminalSignaled)
        {
            m_terminalSignaled = true;
            emit failed(m_failureMessage.isEmpty()
                            ? QStringLiteral("Simulation failed")
                            : m_failureMessage);
        }
        break;
    case TerminalOutcome::None:
        if (!m_terminalSignaled)
        {
            m_terminalSignaled = true;
            emit failed(QStringLiteral(
                "Simulation ended without a terminal outcome"));
        }
        break;
    }
}

void ScenarioRuntime::onExecutorSucceeded()
{
    m_terminalOutcome = TerminalOutcome::Succeeded;
    m_failureMessage.clear();
}

void ScenarioRuntime::onExecutorFailed(const QString &message)
{
    m_terminalOutcome = TerminalOutcome::Failed;
    m_failureMessage =
        message.isEmpty() ? QStringLiteral("Simulation failed")
                          : message;
}

void ScenarioRuntime::cleanupWorker()
{
    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    if (m_executor)
    {
        m_executor->deleteLater();
        m_executor = nullptr;
    }
}

void ScenarioRuntime::stop()
{
    qCInfo(lcScenario) << "ScenarioRuntime::stop: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .stopSimulation();
}

void ScenarioRuntime::pause()
{
    qCInfo(lcScenario) << "ScenarioRuntime::pause: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .pauseSimulation();
}

void ScenarioRuntime::resume()
{
    qCInfo(lcScenario) << "ScenarioRuntime::resume: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .resumeSimulation();
}

double ScenarioRuntime::currentTime() const
{
    return m_lastTime;
}

double ScenarioRuntime::progress() const
{
    return m_lastProgress;
}

bool ScenarioRuntime::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}

QHash<QString, PathMetrics> ScenarioRuntime::actualPathMetrics() const
{
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    return m_lastExecutionResults.actualMetricsByPathIdentity(
        controller.getConfigController());
}

const ScenarioDocument &ScenarioRuntime::document() const
{
    return *m_document;
}

ScenarioDocument &ScenarioRuntime::document()
{
    return *m_document;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
