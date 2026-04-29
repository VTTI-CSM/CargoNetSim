#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

#include "Backend/Commons/TransportationMode.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

enum class ExecutionDemandPolicy
{
    AllocatedOnly,
    DuplicateDemandPerSelectedPath,
    SplitAcrossSelectedPaths
};

enum class ExecutionSchedulingPolicy
{
    ConcurrentPaths,
    SequentialPaths
};

enum class ExecutionIsolationPolicy
{
    SharedSimulatorState,
    IsolatedAlternatives
};

enum class PlannedPathDisposition
{
    Execute,
    SkipNoDemand,
    SkipByPolicy,
    PlanningFailure
};

enum class ResultVisibilityPolicy
{
    PrimaryResults,
    SupplementalStatusOnly,
    Hidden
};

enum class TerminalProcessingRequirement
{
    None,
    RequiredBeforeNextDispatch
};

enum class PathLifecycleState
{
    Pending,
    Running,
    WaitingForTerminalHandoff,
    WaitingForTerminalProcessing,
    ReadyForNextSegment,
    Paused,
    Skipped,
    Completed,
    Failed
};

enum class SegmentLifecycleState
{
    Pending,
    Dispatched,
    VehicleRunning,
    VehicleArrived,
    UnloadCompleted,
    TerminalHandoffCompleted,
    TerminalProcessing,
    ReadyForPickup,
    Skipped,
    Completed,
    Failed
};

enum class ContainerCustodyState
{
    AtOrigin,
    OnVehicle,
    AtTerminal,
    WaitingForTerminalProcessing,
    ReadyForPickup,
    Delivered,
    Failed
};

enum class VehicleLifecycleState
{
    Dispatched,
    VehicleArrived,
    UnloadCompleted,
    TerminalProcessing,
    Completed,
    Failed
};

enum class ExecutionEventType
{
    SegmentVehicleDispatched,
    SegmentVehicleArrived,
    SegmentUnloadSucceeded,
    SegmentUnloadFailed,
    TerminalHandoffSucceeded,
    TerminalHandoffFailed,
    TerminalProcessingReady,
    SegmentExecutionFailed
};

struct TerminalTransitionExpectation
{
    QString scenarioTerminalId;
    bool    requiresTerminalHandoff = true;
    TerminalProcessingRequirement processingRequirement =
        TerminalProcessingRequirement::None;

    bool requiresTerminalProcessing() const
    {
        return processingRequirement
            == TerminalProcessingRequirement::RequiredBeforeNextDispatch;
    }

    bool blocksDownstreamDispatch() const
    {
        return requiresTerminalHandoff
            || requiresTerminalProcessing();
    }
};

struct SegmentExecutionPlan
{
    int    segmentIndex = -1;
    QString segmentId;
    QString pathIdentity;
    QString startTerminalId;
    QString endTerminalId;
    QString regionName;
    QString networkName;
    QString runtimeStartLocationName;
    QString runtimeEndLocationName;
    int     runtimeStartNodeId = -1;
    int     runtimeEndNodeId = -1;
    QPointF startGlobalPosition;
    QPointF endGlobalPosition;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Any;
    TerminalTransitionExpectation terminalTransition;

    bool isWellFormed() const
    {
        const bool baseValid =
            segmentIndex >= 0
            && !segmentId.isEmpty()
            && !pathIdentity.isEmpty()
            && !startTerminalId.isEmpty()
            && !endTerminalId.isEmpty()
            && !regionName.isEmpty()
            && !networkName.isEmpty()
            && mode != TransportationTypes::TransportationMode::Any;

        if (!baseValid)
            return false;

        if (mode == TransportationTypes::TransportationMode::Train
            || mode == TransportationTypes::TransportationMode::Truck)
        {
            const bool nodesValid = runtimeStartNodeId >= 0
                && runtimeEndNodeId >= 0;
            if (mode == TransportationTypes::TransportationMode::Truck)
            {
                return nodesValid
                    && !runtimeStartLocationName.isEmpty()
                    && !runtimeEndLocationName.isEmpty();
            }
            return nodesValid;
        }

        return true;
    }

    bool requiresTerminalReadySignal() const
    {
        return terminalTransition.requiresTerminalProcessing();
    }
};

struct ExecutionTransition
{
    int    fromSegmentIndex = -1;
    int    toSegmentIndex = -1;
    QString handoffTerminalId;
    bool   requiresTerminalHandoff = true;
    TerminalProcessingRequirement processingRequirement =
        TerminalProcessingRequirement::None;

    bool isSequential() const
    {
        return fromSegmentIndex >= 0
            && toSegmentIndex == fromSegmentIndex + 1;
    }
};

struct PathExecutionPlan
{
    QString pathIdentity;
    QString canonicalPathKey;
    int     pathId = -1;
    int     rank = -1;
    QString pathUid;
    QString originId;
    QString destinationId;
    int     effectiveContainerCount = 0;
    QString planningMessage;
    PlannedPathDisposition disposition =
        PlannedPathDisposition::SkipNoDemand;
    ResultVisibilityPolicy resultVisibility =
        ResultVisibilityPolicy::SupplementalStatusOnly;
    QVector<SegmentExecutionPlan> segments;
    QVector<ExecutionTransition> transitions;

    bool isExecutable() const
    {
        return disposition == PlannedPathDisposition::Execute
            && effectiveContainerCount > 0;
    }

    bool hasSequentialSegments() const
    {
        if (segments.isEmpty())
            return true;
        for (int i = 0; i < segments.size(); ++i)
        {
            if (segments[i].segmentIndex != i)
                return false;
        }
        return true;
    }
};

struct ScenarioExecutionPlan
{
    QString executionId;
    ExecutionDemandPolicy demandPolicy =
        ExecutionDemandPolicy::AllocatedOnly;
    ExecutionSchedulingPolicy schedulingPolicy =
        ExecutionSchedulingPolicy::ConcurrentPaths;
    QVector<PathExecutionPlan> paths;

    int executablePathCount() const
    {
        int count = 0;
        for (const auto &path : paths)
        {
            if (path.isExecutable())
                ++count;
        }
        return count;
    }

    bool hasExecutablePaths() const
    {
        return executablePathCount() > 0;
    }

    const PathExecutionPlan *findPath(
        const QString &pathIdentity) const
    {
        for (const auto &path : paths)
        {
            if (path.pathIdentity == pathIdentity)
                return &path;
        }
        return nullptr;
    }
};

struct NetworkExecutionSessionState
{
    QString networkName;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Any;
    bool defined = false;
    bool running = false;
    bool paused = false;
    int  activeSegmentCount = 0;

    bool isActive() const
    {
        return defined && (running || paused);
    }
};

struct ContainerExecutionState
{
    QString containerId;
    QString sourceContainerId;
    QString pathIdentity;
    int     segmentIndex = -1;
    QString currentTerminalId;
    QString currentVehicleId;
    ContainerCustodyState custody =
        ContainerCustodyState::AtOrigin;

    bool isOnVehicle() const
    {
        return custody == ContainerCustodyState::OnVehicle;
    }

    bool isInTerminalCustody() const
    {
        return custody == ContainerCustodyState::AtTerminal
            || custody
                == ContainerCustodyState::WaitingForTerminalProcessing
            || custody == ContainerCustodyState::ReadyForPickup;
    }

    bool isDelivered() const
    {
        return custody == ContainerCustodyState::Delivered;
    }
};

struct VehicleExecutionState
{
    QString vehicleId;
    QString networkName;
    QString lastTerminalId;
    QString lastError;
    VehicleLifecycleState lifecycle =
        VehicleLifecycleState::Dispatched;

    bool isFinished() const
    {
        return lifecycle == VehicleLifecycleState::Completed
            || lifecycle == VehicleLifecycleState::Failed;
    }
};

struct SegmentExecutionState
{
    QString pathIdentity;
    int     segmentIndex = -1;
    SegmentLifecycleState lifecycle =
        SegmentLifecycleState::Pending;
    QString vehicleId;
    QString networkName;
    QString lastTerminalId;
    QString lastError;
    QHash<QString, VehicleExecutionState> vehicleStates;

    bool isTerminalWaitState() const
    {
        return lifecycle
                == SegmentLifecycleState::TerminalHandoffCompleted
            || lifecycle
                == SegmentLifecycleState::TerminalProcessing
            || lifecycle
                == SegmentLifecycleState::ReadyForPickup;
    }

    int dispatchedVehicleCount() const
    {
        return vehicleStates.size();
    }

    int completedVehicleCount() const
    {
        int count = 0;
        for (auto it = vehicleStates.constBegin();
             it != vehicleStates.constEnd(); ++it)
        {
            if (it.value().lifecycle
                == VehicleLifecycleState::Completed)
            {
                ++count;
            }
        }
        return count;
    }

    bool hasVehicle(const QString &vehicleIdToFind) const
    {
        return !vehicleIdToFind.isEmpty()
            && vehicleStates.contains(vehicleIdToFind);
    }
};

struct PathExecutionState
{
    QString pathIdentity;
    PathLifecycleState lifecycle =
        PathLifecycleState::Pending;
    int     activeSegmentIndex = -1;
    QString lastError;

    bool isFinished() const
    {
        return lifecycle == PathLifecycleState::Skipped
            || lifecycle == PathLifecycleState::Completed
            || lifecycle == PathLifecycleState::Failed;
    }

    bool isSkipped() const
    {
        return lifecycle == PathLifecycleState::Skipped;
    }

    bool isTerminalWaitState() const
    {
        return lifecycle
                == PathLifecycleState::WaitingForTerminalHandoff
            || lifecycle
                == PathLifecycleState::WaitingForTerminalProcessing;
    }
};

struct DispatchableSegmentRef
{
    QString pathIdentity;
    int     segmentIndex = -1;

    bool isValid() const
    {
        return !pathIdentity.isEmpty() && segmentIndex >= 0;
    }
};

struct ExecutionEventOutcome
{
    bool accepted = false;
    bool duplicate = false;
    bool advancedState = false;
    QString errorMessage;
    QVector<DispatchableSegmentRef> newlyDispatchableSegments;
};

struct ExecutionEvent
{
    ExecutionEventType type =
        ExecutionEventType::SegmentExecutionFailed;
    QString pathIdentity;
    int     segmentIndex = -1;
    QString vehicleId;
    QString networkName;
    QString terminalId;
    QString correlationToken;
    QString message;
    double  eventTimeSeconds = -1.0;

    bool isFailure() const
    {
        return type == ExecutionEventType::SegmentUnloadFailed
            || type == ExecutionEventType::TerminalHandoffFailed
            || type == ExecutionEventType::SegmentExecutionFailed;
    }
};

struct SegmentTimelineWindow
{
    int    segmentIndex = -1;
    double dispatchedAtSeconds = -1.0;
    double vehicleArrivedAtSeconds = -1.0;
    double unloadCompletedAtSeconds = -1.0;
    double terminalReadyAtSeconds = -1.0;
    double completedAtSeconds = -1.0;
};

struct ExecutionTimeline
{
    QHash<QString, QVector<SegmentTimelineWindow>>
        segmentWindowsByPath;

    bool hasPath(const QString &pathIdentity) const
    {
        return segmentWindowsByPath.contains(pathIdentity);
    }
};

struct ExecutionLedger
{
    QHash<QString, PathExecutionState> pathStates;
    QHash<QString, QVector<SegmentExecutionState>> segmentStates;
    QHash<QString, QVector<ContainerExecutionState>> containerStates;
    ExecutionTimeline timeline;

    bool containsPath(const QString &pathIdentity) const
    {
        return pathStates.contains(pathIdentity);
    }
};

struct CoordinatorSnapshot
{
    ScenarioExecutionPlan plan;
    ExecutionLedger       ledger;
    QVector<DispatchableSegmentRef> dispatchableSegments;
};

struct SegmentProgressSnapshot
{
    int segmentIndex = -1;
    QString segmentId;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Any;
    QString networkName;
    QString startTerminalId;
    QString endTerminalId;
    SegmentLifecycleState lifecycle =
        SegmentLifecycleState::Pending;
    int    dispatchedVehicleCount = 0;
    int    completedVehicleCount = 0;
    double percent = 0.0;
    bool   active = false;
};

struct PathProgressSnapshot
{
    QString pathIdentity;
    QString canonicalPathKey;
    int     pathId = -1;
    int     rank = -1;
    QString originId;
    QString destinationId;
    PlannedPathDisposition disposition =
        PlannedPathDisposition::SkipNoDemand;
    PathLifecycleState lifecycle =
        PathLifecycleState::Pending;
    int activeSegmentIndex = -1;
    TransportationTypes::TransportationMode activeMode =
        TransportationTypes::TransportationMode::Any;
    QString activeNetworkName;
    QString activeStartTerminalId;
    QString activeEndTerminalId;
    int     completedSegments = 0;
    int     totalSegments = 0;
    double  percent = 0.0;
    bool    executable = false;
    QString message;
    QVector<SegmentProgressSnapshot> segments;
};

struct ExecutionProgressSnapshot
{
    double aggregatePercent = 0.0;
    int    activeAlternativeIndex = -1;
    int    alternativeCount = 0;
    int    completedExecutablePaths = 0;
    int    executablePathCount = 0;
    int    completedExecutableSegments = 0;
    int    executableSegmentCount = 0;
    QVector<PathProgressSnapshot> paths;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim

Q_DECLARE_METATYPE(CargoNetSim::Backend::Scenario::ExecutionEvent)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::Scenario::SegmentProgressSnapshot)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::Scenario::PathProgressSnapshot)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::Scenario::ExecutionProgressSnapshot)
