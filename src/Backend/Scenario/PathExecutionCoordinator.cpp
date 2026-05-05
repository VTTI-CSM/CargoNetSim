#include "PathExecutionCoordinator.h"

#include "ExecutionContainerIdentity.h"

#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

int lifecycleRank(VehicleLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case VehicleLifecycleState::Dispatched:
        return 0;
    case VehicleLifecycleState::VehicleArrived:
        return 1;
    case VehicleLifecycleState::UnloadCompleted:
        return 2;
    case VehicleLifecycleState::TerminalProcessing:
        return 3;
    case VehicleLifecycleState::Completed:
        return 4;
    case VehicleLifecycleState::Failed:
        return 5;
    }

    return -1;
}

bool hasAnyVehicleInLifecycle(const SegmentExecutionState &segmentState,
                              VehicleLifecycleState        lifecycle)
{
    for (auto it = segmentState.vehicleStates.constBegin();
         it != segmentState.vehicleStates.constEnd(); ++it)
    {
        if (it.value().lifecycle == lifecycle)
            return true;
    }
    return false;
}

bool allVehiclesInLifecycleAtLeast(
    const SegmentExecutionState &segmentState,
    VehicleLifecycleState        lifecycle)
{
    if (segmentState.vehicleStates.isEmpty())
        return false;

    const int minimumRank = lifecycleRank(lifecycle);
    for (auto it = segmentState.vehicleStates.constBegin();
         it != segmentState.vehicleStates.constEnd(); ++it)
    {
        if (lifecycleRank(it.value().lifecycle) < minimumRank)
            return false;
    }
    return true;
}

void recordFirstDispatchTime(double &target, double value)
{
    if (value < 0.0)
        return;

    if (target < 0.0 || value < target)
        target = value;
}

void recordLatestProgressTime(double &target, double value)
{
    if (value < 0.0)
        return;

    if (target < 0.0 || value > target)
        target = value;
}

ContainerCustodyState completedCustodyFor(
    const PathExecutionPlan &pathPlan, int segmentIndex)
{
    return segmentIndex + 1 < pathPlan.segments.size()
               ? ContainerCustodyState::ReadyForPickup
               : ContainerCustodyState::Delivered;
}

int stageVehicleContainersAtTerminal(
    ExecutionLedger         &ledger,
    const PathExecutionPlan &pathPlan, int segmentIndex,
    const QString &vehicleId,
    ContainerCustodyState custody)
{
    auto it = ledger.containerStates.find(pathPlan.executionPathKey);
    if (it == ledger.containerStates.end() || vehicleId.isEmpty())
        return 0;

    const auto &segmentPlan = pathPlan.segments[segmentIndex];
    const int stagedSegmentIndex =
        segmentIndex + 1 < pathPlan.segments.size()
            ? segmentIndex + 1
            : segmentIndex;

    int moved = 0;
    for (auto &state : it.value())
    {
        if (state.segmentIndex != segmentIndex
            || state.currentVehicleId != vehicleId
            || state.custody != ContainerCustodyState::OnVehicle)
        {
            continue;
        }

        state.currentTerminalId = segmentPlan.endTerminalId;
        state.segmentIndex = stagedSegmentIndex;
        state.custody = custody;
        if (custody != ContainerCustodyState::WaitingForTerminalProcessing)
            state.currentVehicleId.clear();
        ++moved;
    }

    return moved;
}

int completeTerminalProcessingForVehicle(
    ExecutionLedger         &ledger,
    const PathExecutionPlan &pathPlan, int segmentIndex,
    const QString &vehicleId)
{
    auto it = ledger.containerStates.find(pathPlan.executionPathKey);
    if (it == ledger.containerStates.end() || vehicleId.isEmpty())
        return 0;

    const auto &segmentPlan = pathPlan.segments[segmentIndex];
    const int stagedSegmentIndex =
        segmentIndex + 1 < pathPlan.segments.size()
            ? segmentIndex + 1
            : segmentIndex;
    const ContainerCustodyState nextCustody =
        completedCustodyFor(pathPlan, segmentIndex);

    int moved = 0;
    for (auto &state : it.value())
    {
        if (state.segmentIndex != stagedSegmentIndex
            || state.currentVehicleId != vehicleId
            || state.custody
                   != ContainerCustodyState::WaitingForTerminalProcessing)
        {
            continue;
        }

        state.currentTerminalId = segmentPlan.endTerminalId;
        state.custody = nextCustody;
        state.currentVehicleId.clear();
        ++moved;
    }

    return moved;
}

int failUndeliveredContainers(ExecutionLedger &ledger,
                              const QString   &executionPathKey,
                              const QString   &terminalId)
{
    auto it = ledger.containerStates.find(executionPathKey);
    if (it == ledger.containerStates.end())
        return 0;

    int changed = 0;
    for (auto &state : it.value())
    {
        if (state.custody == ContainerCustodyState::Delivered
            || state.custody == ContainerCustodyState::Failed)
        {
            continue;
        }

        state.custody = ContainerCustodyState::Failed;
        if (!terminalId.isEmpty())
        {
            state.currentTerminalId = terminalId;
        }
        ++changed;
    }

    return changed;
}

} // namespace

bool PathExecutionCoordinator::initialize(
    const ScenarioExecutionPlan &plan, QString *err)
{
    m_plan = plan;
    m_ledger = ExecutionLedger{};
    m_dispatchableSegments.clear();
    m_processedEventKeys.clear();

    if (m_plan.executionId.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                "Execution coordinator requires a plan with a non-empty execution ID");
        }
        return false;
    }

    for (const auto &pathPlan : m_plan.paths)
    {
        PathExecutionState pathState;
        pathState.executionPathKey = pathPlan.executionPathKey;

        QVector<SegmentExecutionState> segmentStates;
        segmentStates.reserve(pathPlan.segments.size());

        QVector<SegmentTimelineWindow> timelineWindows;
        timelineWindows.reserve(pathPlan.segments.size());

        if (pathPlan.disposition == PlannedPathDisposition::Execute)
        {
            if (pathPlan.segments.isEmpty())
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Executable path %1 has no segments")
                               .arg(pathPlan.executionPathKey);
                }
                return false;
            }

            pathState.lifecycle = PathLifecycleState::Pending;
            pathState.activeSegmentIndex = 0;
        }
        else if (pathPlan.disposition
                 == PlannedPathDisposition::PlanningFailure)
        {
            pathState.lifecycle = PathLifecycleState::Failed;
            pathState.lastError = pathPlan.planningMessage;
            pathState.activeSegmentIndex = -1;
        }
        else
        {
            pathState.lifecycle = PathLifecycleState::Skipped;
            pathState.lastError = pathPlan.planningMessage;
            pathState.activeSegmentIndex = -1;
        }

        for (const auto &segmentPlan : pathPlan.segments)
        {
            SegmentExecutionState segmentState;
            segmentState.executionPathKey = pathPlan.executionPathKey;
            segmentState.segmentIndex = segmentPlan.segmentIndex;
            segmentState.networkName = segmentPlan.networkName;
            if (pathPlan.disposition != PlannedPathDisposition::Execute)
            {
                segmentState.lifecycle = SegmentLifecycleState::Skipped;
                segmentState.lastError = pathPlan.planningMessage;
            }
            segmentStates.append(segmentState);

            SegmentTimelineWindow timelineWindow;
            timelineWindow.segmentIndex = segmentPlan.segmentIndex;
            timelineWindows.append(timelineWindow);
        }

        m_ledger.pathStates.insert(pathPlan.executionPathKey, pathState);
        m_ledger.segmentStates.insert(pathPlan.executionPathKey, segmentStates);
        m_ledger.timeline.segmentWindowsByPath.insert(
            pathPlan.executionPathKey, timelineWindows);
    }

    m_dispatchableSegments = recomputeDispatchableSegments();
    if (err)
        err->clear();
    return true;
}

CoordinatorSnapshot PathExecutionCoordinator::snapshot() const
{
    return { m_plan, m_ledger, m_dispatchableSegments };
}

bool PathExecutionCoordinator::seedContainers(
    const PathAllocation &allocation, QString *err)
{
    m_ledger.containerStates.clear();

    for (const auto &pathPlan : m_plan.paths)
    {
        QVector<ContainerExecutionState> states;

        if (pathPlan.disposition == PlannedPathDisposition::Execute)
        {
            const auto allocatedContainers =
                allocation.byCanonicalPath.value(
                    pathPlan.canonicalPathKey);
            if (allocatedContainers.size()
                != pathPlan.effectiveContainerCount)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Container seed count mismatch for %1: expected %2, got %3")
                               .arg(pathPlan.executionPathKey)
                               .arg(pathPlan.effectiveContainerCount)
                               .arg(allocatedContainers.size());
                }
                return false;
            }

            states.reserve(allocatedContainers.size());
            for (const auto *container : allocatedContainers)
            {
                if (!container)
                    continue;

                ContainerExecutionState state;
                const auto metadata =
                    ExecutionContainers::makeIdentityMetadata(
                        m_plan.executionId,
                        pathPlan.executionPathKey,
                        pathPlan.canonicalPathKey,
                        *container,
                        /*readySegmentIndex=*/0,
                        /*terminalSequenceIndex=*/0);
                state.containerId = metadata.executionContainerId;
                state.sourceContainerId = metadata.sourceContainerId;
                state.executionPathKey = pathPlan.executionPathKey;
                state.segmentIndex = 0;
                state.currentTerminalId = pathPlan.originId;
                state.custody = ContainerCustodyState::AtOrigin;
                states.append(state);
            }
        }

        m_ledger.containerStates.insert(pathPlan.executionPathKey,
                                        states);
    }

    if (err)
        err->clear();
    return true;
}

bool PathExecutionCoordinator::registerDispatchAssignments(
    const QVector<VehicleDispatchAssignment> &assignments,
    QString                                  *err)
{
    for (const auto &assignment : assignments)
    {
        const PathExecutionPlan *pathPlan =
            findPathPlan(assignment.executionPathKey);
        if (!pathPlan)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Unknown execution path key in dispatch assignment: %1")
                           .arg(assignment.executionPathKey);
            }
            return false;
        }

        if (assignment.segmentIndex < 0
            || assignment.segmentIndex >= pathPlan->segments.size())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Invalid segment index %1 in dispatch assignment for %2")
                           .arg(assignment.segmentIndex)
                           .arg(assignment.executionPathKey);
            }
            return false;
        }

        const auto &segmentPlan =
            pathPlan->segments[assignment.segmentIndex];
        auto containerStates =
            m_ledger.containerStates.find(assignment.executionPathKey);
        if (containerStates == m_ledger.containerStates.end())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Container state ledger is missing path %1")
                           .arg(assignment.executionPathKey);
            }
            return false;
        }

        for (const QString &logicalContainerId :
             assignment.logicalContainerIds)
        {
            ContainerExecutionState *state =
                findContainerState(assignment.executionPathKey,
                                   logicalContainerId);
            if (!state)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Dispatch assignment references unknown logical container %1 for %2")
                               .arg(logicalContainerId,
                                    assignment.executionPathKey);
                }
                return false;
            }

            if (state->segmentIndex != assignment.segmentIndex)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Logical container %1 is not eligible for segment %2")
                               .arg(logicalContainerId)
                               .arg(assignment.segmentIndex);
                }
                return false;
            }

            if (state->currentTerminalId
                != segmentPlan.startTerminalId)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Logical container %1 is not staged at terminal %2")
                               .arg(logicalContainerId,
                                    segmentPlan.startTerminalId);
                }
                return false;
            }

            if (state->custody != ContainerCustodyState::AtOrigin
                && state->custody
                       != ContainerCustodyState::ReadyForPickup)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Logical container %1 is not dispatchable from custody state %2")
                               .arg(logicalContainerId)
                               .arg(static_cast<int>(state->custody));
                }
                return false;
            }

            state->custody = ContainerCustodyState::OnVehicle;
            state->currentVehicleId = assignment.vehicleId;
            state->segmentIndex = assignment.segmentIndex;
        }
    }

    if (err)
        err->clear();
    return true;
}

ExecutionEventOutcome PathExecutionCoordinator::markSegmentDispatched(
    const DispatchableSegmentRef &segmentRef, const QString &vehicleId,
    const QString &networkName, double eventTimeSeconds)
{
    ExecutionEvent event;
    event.type = ExecutionEventType::SegmentVehicleDispatched;
    event.executionPathKey = segmentRef.executionPathKey;
    event.segmentIndex = segmentRef.segmentIndex;
    event.vehicleId = vehicleId;
    event.networkName = networkName;
    event.eventTimeSeconds = eventTimeSeconds;
    return applyEvent(event);
}

ExecutionEventOutcome PathExecutionCoordinator::applyEvent(
    const ExecutionEvent &event)
{
    ExecutionEventOutcome outcome;

    const PathExecutionPlan *pathPlan =
        findPathPlan(event.executionPathKey);
    PathExecutionState *pathState =
        findPathState(event.executionPathKey);
    SegmentExecutionState *segmentState =
        findSegmentState(event.executionPathKey, event.segmentIndex);
    SegmentTimelineWindow *timeline =
        findTimelineWindow(event.executionPathKey, event.segmentIndex);

    if (!pathPlan || !pathState || !segmentState || !timeline)
    {
        outcome.errorMessage = QStringLiteral(
            "Execution event references an unknown path or segment");
        return outcome;
    }

    const QString key = eventKey(event);
    if (m_processedEventKeys.contains(key))
    {
        outcome.accepted = true;
        outcome.duplicate = true;
        outcome.newlyDispatchableSegments = m_dispatchableSegments;
        return outcome;
    }

    if (pathState->isFinished())
    {
        outcome.errorMessage = QStringLiteral(
            "Execution event arrived after the path had already finished");
        return outcome;
    }

    auto acceptEvent = [&]() {
        m_processedEventKeys.insert(key);
        m_dispatchableSegments = recomputeDispatchableSegments();
        outcome.accepted = true;
        outcome.newlyDispatchableSegments = m_dispatchableSegments;
    };

    auto requireVehicleId = [&]() -> bool {
        if (!event.vehicleId.trimmed().isEmpty())
            return true;

        outcome.errorMessage = QStringLiteral(
            "Execution event is missing the vehicle identifier required for multi-vehicle segment tracking");
        return false;
    };

    switch (event.type)
    {
    case ExecutionEventType::SegmentVehicleDispatched:
    {
        if (!requireVehicleId())
            return outcome;

        if (pathState->activeSegmentIndex != event.segmentIndex
            && pathState->lifecycle != PathLifecycleState::Pending
            && pathState->lifecycle
                   != PathLifecycleState::ReadyForNextSegment)
        {
            outcome.errorMessage = QStringLiteral(
                "Segment dispatch event does not match the active path segment");
            return outcome;
        }

        if (segmentState->lifecycle == SegmentLifecycleState::Skipped
            || segmentState->lifecycle == SegmentLifecycleState::Failed
            || segmentState->lifecycle == SegmentLifecycleState::Completed)
        {
            outcome.errorMessage = QStringLiteral(
                "Segment dispatch event references a non-dispatchable segment state");
            return outcome;
        }

        if (segmentState->hasVehicle(event.vehicleId))
        {
            outcome.accepted = true;
            outcome.duplicate = true;
            outcome.newlyDispatchableSegments = m_dispatchableSegments;
            return outcome;
        }

        VehicleExecutionState vehicleState;
        vehicleState.vehicleId = event.vehicleId;
        vehicleState.networkName =
            event.networkName.isEmpty()
                ? segmentState->networkName
                : event.networkName;
        segmentState->vehicleStates.insert(event.vehicleId, vehicleState);
        segmentState->vehicleId = event.vehicleId;
        if (!event.networkName.isEmpty())
            segmentState->networkName = event.networkName;
        pathState->lifecycle = PathLifecycleState::Running;
        pathState->activeSegmentIndex = event.segmentIndex;
        recordFirstDispatchTime(timeline->dispatchedAtSeconds,
                                event.eventTimeSeconds);
        refreshSegmentState(*pathPlan, *segmentState, *pathState);
        outcome.advancedState = true;
        acceptEvent();
        return outcome;
    }

    case ExecutionEventType::SegmentVehicleArrived:
    {
        if (!requireVehicleId())
            return outcome;

        VehicleExecutionState *vehicleState =
            findVehicleState(event.executionPathKey, event.segmentIndex,
                             event.vehicleId);
        if (!vehicleState)
        {
            outcome.errorMessage = QStringLiteral(
                "Vehicle arrival event references an unknown dispatched vehicle");
            return outcome;
        }

        if (lifecycleRank(vehicleState->lifecycle)
            >= lifecycleRank(VehicleLifecycleState::VehicleArrived))
        {
            outcome.accepted = true;
            outcome.duplicate = true;
            outcome.newlyDispatchableSegments = m_dispatchableSegments;
            return outcome;
        }

        vehicleState->lifecycle = VehicleLifecycleState::VehicleArrived;
        vehicleState->lastTerminalId = event.terminalId;
        segmentState->vehicleId = event.vehicleId;
        if (!event.terminalId.isEmpty())
            segmentState->lastTerminalId = event.terminalId;
        recordLatestProgressTime(timeline->vehicleArrivedAtSeconds,
                                 event.eventTimeSeconds);
        refreshSegmentState(*pathPlan, *segmentState, *pathState);
        outcome.advancedState = true;
        acceptEvent();
        return outcome;
    }

    case ExecutionEventType::SegmentUnloadSucceeded:
    {
        if (!requireVehicleId())
            return outcome;

        VehicleExecutionState *vehicleState =
            findVehicleState(event.executionPathKey, event.segmentIndex,
                             event.vehicleId);
        if (!vehicleState)
        {
            outcome.errorMessage = QStringLiteral(
                "Unload success event references an unknown dispatched vehicle");
            return outcome;
        }

        if (lifecycleRank(vehicleState->lifecycle)
            >= lifecycleRank(VehicleLifecycleState::UnloadCompleted))
        {
            outcome.accepted = true;
            outcome.duplicate = true;
            outcome.newlyDispatchableSegments = m_dispatchableSegments;
            return outcome;
        }

        if (vehicleState->lifecycle
            != VehicleLifecycleState::VehicleArrived)
        {
            outcome.errorMessage = QStringLiteral(
                "Unload success event is only valid after vehicle arrival");
            return outcome;
        }

        vehicleState->lifecycle = VehicleLifecycleState::UnloadCompleted;
        recordLatestProgressTime(timeline->unloadCompletedAtSeconds,
                                 event.eventTimeSeconds);

        const auto &segmentPlan =
            pathPlan->segments[event.segmentIndex];
        if (!segmentPlan.terminalTransition.requiresTerminalHandoff)
        {
            const ContainerCustodyState nextCustody =
                segmentPlan.requiresTerminalReadySignal()
                    ? ContainerCustodyState::WaitingForTerminalProcessing
                    : completedCustodyFor(*pathPlan,
                                          event.segmentIndex);
            if (stageVehicleContainersAtTerminal(
                    m_ledger, *pathPlan, event.segmentIndex,
                    event.vehicleId, nextCustody)
                <= 0)
            {
                outcome.errorMessage = QStringLiteral(
                    "Unload success event did not match any in-flight logical containers");
                return outcome;
            }

            if (segmentPlan.requiresTerminalReadySignal())
            {
                vehicleState->lifecycle =
                    VehicleLifecycleState::TerminalProcessing;
            }
            else
            {
                vehicleState->lifecycle =
                    VehicleLifecycleState::Completed;
            }
        }

        refreshSegmentState(*pathPlan, *segmentState, *pathState);
        if (segmentReadyForCompletion(*segmentState))
        {
            outcome = completeSegment(event.executionPathKey,
                                      event.segmentIndex,
                                      event.eventTimeSeconds);
            if (outcome.accepted)
                m_processedEventKeys.insert(key);
            return outcome;
        }

        outcome.advancedState = true;
        acceptEvent();
        return outcome;
    }

    case ExecutionEventType::TerminalHandoffSucceeded:
    {
        if (!requireVehicleId())
            return outcome;

        VehicleExecutionState *vehicleState =
            findVehicleState(event.executionPathKey, event.segmentIndex,
                             event.vehicleId);
        if (!vehicleState)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal handoff success references an unknown dispatched vehicle");
            return outcome;
        }

        if (vehicleState->lifecycle
            == VehicleLifecycleState::Completed)
        {
            outcome.accepted = true;
            outcome.duplicate = true;
            outcome.newlyDispatchableSegments = m_dispatchableSegments;
            return outcome;
        }

        if (vehicleState->lifecycle
            != VehicleLifecycleState::UnloadCompleted)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal handoff success is only valid after unload completion");
            return outcome;
        }

        vehicleState->lastTerminalId = event.terminalId;
        segmentState->vehicleId = event.vehicleId;
        if (!event.terminalId.isEmpty())
            segmentState->lastTerminalId = event.terminalId;

        const auto &segmentPlan =
            pathPlan->segments[event.segmentIndex];
        const ContainerCustodyState nextCustody =
            segmentPlan.requiresTerminalReadySignal()
                ? ContainerCustodyState::WaitingForTerminalProcessing
                : completedCustodyFor(*pathPlan,
                                      event.segmentIndex);
        if (stageVehicleContainersAtTerminal(
                m_ledger, *pathPlan, event.segmentIndex,
                event.vehicleId, nextCustody)
            <= 0)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal handoff success did not match any in-flight logical containers");
            return outcome;
        }

        if (segmentPlan.requiresTerminalReadySignal())
        {
            vehicleState->lifecycle =
                VehicleLifecycleState::TerminalProcessing;
        }
        else
        {
            vehicleState->lifecycle =
                VehicleLifecycleState::Completed;
        }

        refreshSegmentState(*pathPlan, *segmentState, *pathState);
        if (segmentReadyForCompletion(*segmentState))
        {
            outcome = completeSegment(event.executionPathKey,
                                      event.segmentIndex,
                                      event.eventTimeSeconds);
            if (outcome.accepted)
                m_processedEventKeys.insert(key);
            return outcome;
        }

        outcome.advancedState = true;
        acceptEvent();
        return outcome;
    }

    case ExecutionEventType::TerminalProcessingReady:
    {
        if (!requireVehicleId())
            return outcome;

        VehicleExecutionState *vehicleState =
            findVehicleState(event.executionPathKey, event.segmentIndex,
                             event.vehicleId);
        if (!vehicleState)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal ready event references an unknown dispatched vehicle");
            return outcome;
        }

        if (vehicleState->lifecycle
            == VehicleLifecycleState::Completed)
        {
            outcome.accepted = true;
            outcome.duplicate = true;
            outcome.newlyDispatchableSegments = m_dispatchableSegments;
            return outcome;
        }

        if (vehicleState->lifecycle
            != VehicleLifecycleState::TerminalProcessing)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal ready event is only valid while terminal processing is pending");
            return outcome;
        }

        if (completeTerminalProcessingForVehicle(
                m_ledger, *pathPlan, event.segmentIndex,
                event.vehicleId)
            <= 0)
        {
            outcome.errorMessage = QStringLiteral(
                "Terminal ready event did not match any waiting logical containers");
            return outcome;
        }

        vehicleState->lifecycle = VehicleLifecycleState::Completed;
        recordLatestProgressTime(timeline->terminalReadyAtSeconds,
                                 event.eventTimeSeconds);
        refreshSegmentState(*pathPlan, *segmentState, *pathState);
        if (segmentReadyForCompletion(*segmentState))
        {
            outcome = completeSegment(event.executionPathKey,
                                      event.segmentIndex,
                                      event.eventTimeSeconds);
            if (outcome.accepted)
                m_processedEventKeys.insert(key);
            return outcome;
        }

        outcome.advancedState = true;
        acceptEvent();
        return outcome;
    }

    case ExecutionEventType::SegmentUnloadFailed:
    case ExecutionEventType::TerminalHandoffFailed:
    case ExecutionEventType::SegmentExecutionFailed:
        outcome = failPath(event, event.message.isEmpty()
                                      ? QStringLiteral("Segment execution failed")
                                      : event.message);
        if (outcome.accepted)
            m_processedEventKeys.insert(key);
        return outcome;
    }

    outcome.errorMessage =
        QStringLiteral("Unsupported execution event type");
    return outcome;
}

const PathExecutionPlan *PathExecutionCoordinator::findPathPlan(
    const QString &executionPathKey) const
{
    return m_plan.findPath(executionPathKey);
}

PathExecutionState *PathExecutionCoordinator::findPathState(
    const QString &executionPathKey)
{
    auto it = m_ledger.pathStates.find(executionPathKey);
    return it == m_ledger.pathStates.end() ? nullptr : &it.value();
}

SegmentExecutionState *PathExecutionCoordinator::findSegmentState(
    const QString &executionPathKey, int segmentIndex)
{
    auto it = m_ledger.segmentStates.find(executionPathKey);
    if (it == m_ledger.segmentStates.end()
        || segmentIndex < 0
        || segmentIndex >= it.value().size())
    {
        return nullptr;
    }
    return &it.value()[segmentIndex];
}

VehicleExecutionState *PathExecutionCoordinator::findVehicleState(
    const QString &executionPathKey, int segmentIndex,
    const QString &vehicleId)
{
    SegmentExecutionState *segmentState =
        findSegmentState(executionPathKey, segmentIndex);
    if (!segmentState || vehicleId.isEmpty())
        return nullptr;

    auto it = segmentState->vehicleStates.find(vehicleId);
    return it == segmentState->vehicleStates.end()
               ? nullptr
               : &it.value();
}

SegmentTimelineWindow *PathExecutionCoordinator::findTimelineWindow(
    const QString &executionPathKey, int segmentIndex)
{
    auto it = m_ledger.timeline.segmentWindowsByPath.find(executionPathKey);
    if (it == m_ledger.timeline.segmentWindowsByPath.end()
        || segmentIndex < 0
        || segmentIndex >= it.value().size())
    {
        return nullptr;
    }
    return &it.value()[segmentIndex];
}

ContainerExecutionState *PathExecutionCoordinator::findContainerState(
    const QString &executionPathKey,
    const QString &logicalContainerId)
{
    auto it = m_ledger.containerStates.find(executionPathKey);
    if (it == m_ledger.containerStates.end()
        || logicalContainerId.isEmpty())
    {
        return nullptr;
    }

    for (auto &state : it.value())
    {
        if (state.containerId == logicalContainerId)
            return &state;
    }
    return nullptr;
}

void PathExecutionCoordinator::refreshSegmentState(
    const PathExecutionPlan &pathPlan,
    SegmentExecutionState   &segmentState,
    PathExecutionState      &pathState)
{
    if (segmentState.lifecycle == SegmentLifecycleState::Skipped
        || segmentState.lifecycle == SegmentLifecycleState::Failed
        || segmentState.lifecycle == SegmentLifecycleState::Completed)
    {
        return;
    }

    if (segmentState.vehicleStates.isEmpty())
    {
        segmentState.lifecycle = SegmentLifecycleState::Pending;
        return;
    }

    if (hasAnyVehicleInLifecycle(segmentState,
                                 VehicleLifecycleState::Failed))
    {
        segmentState.lifecycle = SegmentLifecycleState::Failed;
        pathState.lifecycle = PathLifecycleState::Failed;
        pathState.activeSegmentIndex = segmentState.segmentIndex;
        return;
    }

    if (allVehiclesInLifecycleAtLeast(
            segmentState, VehicleLifecycleState::Completed))
    {
        segmentState.lifecycle = SegmentLifecycleState::Completed;
        return;
    }

    if (hasAnyVehicleInLifecycle(segmentState,
                                 VehicleLifecycleState::TerminalProcessing))
    {
        segmentState.lifecycle = SegmentLifecycleState::TerminalProcessing;
        pathState.lifecycle =
            PathLifecycleState::WaitingForTerminalProcessing;
        pathState.activeSegmentIndex = segmentState.segmentIndex;
        return;
    }

    if (hasAnyVehicleInLifecycle(segmentState,
                                 VehicleLifecycleState::UnloadCompleted))
    {
        segmentState.lifecycle = SegmentLifecycleState::UnloadCompleted;
        pathState.lifecycle =
            pathPlan.segments[segmentState.segmentIndex]
                    .terminalTransition.requiresTerminalHandoff
                ? PathLifecycleState::WaitingForTerminalHandoff
                : PathLifecycleState::Running;
        pathState.activeSegmentIndex = segmentState.segmentIndex;
        return;
    }

    if (hasAnyVehicleInLifecycle(segmentState,
                                 VehicleLifecycleState::VehicleArrived))
    {
        segmentState.lifecycle = SegmentLifecycleState::VehicleArrived;
        pathState.lifecycle = PathLifecycleState::Running;
        pathState.activeSegmentIndex = segmentState.segmentIndex;
        return;
    }

    segmentState.lifecycle = SegmentLifecycleState::Dispatched;
    pathState.lifecycle = PathLifecycleState::Running;
    pathState.activeSegmentIndex = segmentState.segmentIndex;
}

bool PathExecutionCoordinator::segmentReadyForCompletion(
    const SegmentExecutionState &segmentState) const
{
    return !segmentState.vehicleStates.isEmpty()
        && allVehiclesInLifecycleAtLeast(
            segmentState, VehicleLifecycleState::Completed);
}

QVector<DispatchableSegmentRef>
PathExecutionCoordinator::recomputeDispatchableSegments() const
{
    QVector<DispatchableSegmentRef> dispatchable;

    auto appendIfReady = [&](const PathExecutionPlan &pathPlan) -> bool {
        const auto pathStateIt =
            m_ledger.pathStates.constFind(pathPlan.executionPathKey);
        const auto segmentStatesIt =
            m_ledger.segmentStates.constFind(pathPlan.executionPathKey);
        if (pathStateIt == m_ledger.pathStates.constEnd()
            || segmentStatesIt == m_ledger.segmentStates.constEnd())
        {
            return false;
        }

        const auto &pathState = pathStateIt.value();
        if (pathState.lifecycle != PathLifecycleState::Pending
            && pathState.lifecycle
                   != PathLifecycleState::ReadyForNextSegment)
        {
            return false;
        }

        const int segmentIndex = pathState.activeSegmentIndex;
        if (segmentIndex < 0
            || segmentIndex >= segmentStatesIt.value().size())
        {
            return false;
        }

        if (segmentStatesIt.value().at(segmentIndex).lifecycle
            != SegmentLifecycleState::Pending)
        {
            return false;
        }

        dispatchable.append(
            { pathPlan.executionPathKey, segmentIndex });
        return true;
    };

    if (m_plan.schedulingPolicy
        == ExecutionSchedulingPolicy::SequentialPaths)
    {
        for (const auto &pathPlan : m_plan.paths)
        {
            if (pathPlan.disposition
                != PlannedPathDisposition::Execute)
            {
                continue;
            }

            const auto pathStateIt =
                m_ledger.pathStates.constFind(
                    pathPlan.executionPathKey);
            if (pathStateIt == m_ledger.pathStates.constEnd())
                continue;

            const auto lifecycle = pathStateIt.value().lifecycle;
            if (lifecycle == PathLifecycleState::Completed
                || lifecycle == PathLifecycleState::Skipped
                || lifecycle == PathLifecycleState::Failed)
            {
                continue;
            }

            appendIfReady(pathPlan);
            break;
        }

        return dispatchable;
    }

    for (const auto &pathPlan : m_plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        appendIfReady(pathPlan);
    }
    return dispatchable;
}

QString PathExecutionCoordinator::eventKey(
    const ExecutionEvent &event) const
{
    if (!event.correlationToken.isEmpty())
    {
        return QStringLiteral("%1|%2")
            .arg(QString::number(static_cast<int>(event.type)),
                 event.correlationToken);
    }

    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(QString::number(static_cast<int>(event.type)),
             event.executionPathKey,
             QString::number(event.segmentIndex),
             event.vehicleId,
             event.networkName,
             event.terminalId,
             event.message);
}

ExecutionEventOutcome PathExecutionCoordinator::failPath(
    const ExecutionEvent &event, const QString &message)
{
    ExecutionEventOutcome outcome;

    PathExecutionState *pathState = findPathState(event.executionPathKey);
    SegmentExecutionState *segmentState =
        findSegmentState(event.executionPathKey, event.segmentIndex);
    if (!pathState || !segmentState)
    {
        outcome.errorMessage = QStringLiteral(
            "Failure event references an unknown path or segment");
        return outcome;
    }

    pathState->lifecycle = PathLifecycleState::Failed;
    pathState->lastError = message;
    segmentState->lifecycle = SegmentLifecycleState::Failed;
    segmentState->lastError = message;
    if (!event.vehicleId.isEmpty())
    {
        VehicleExecutionState vehicleState;
        vehicleState.vehicleId = event.vehicleId;
        vehicleState.networkName = event.networkName;
        vehicleState.lastTerminalId = event.terminalId;
        vehicleState.lastError = message;
        vehicleState.lifecycle = VehicleLifecycleState::Failed;
        segmentState->vehicleStates.insert(event.vehicleId, vehicleState);
    }
    failUndeliveredContainers(m_ledger, event.executionPathKey,
                              event.terminalId);

    m_dispatchableSegments = recomputeDispatchableSegments();
    outcome.accepted = true;
    outcome.advancedState = true;
    outcome.newlyDispatchableSegments = m_dispatchableSegments;
    return outcome;
}

ExecutionEventOutcome PathExecutionCoordinator::completeSegment(
    const QString &executionPathKey, int segmentIndex,
    double eventTimeSeconds)
{
    ExecutionEventOutcome outcome;

    PathExecutionState *pathState = findPathState(executionPathKey);
    SegmentExecutionState *segmentState =
        findSegmentState(executionPathKey, segmentIndex);
    SegmentTimelineWindow *timeline =
        findTimelineWindow(executionPathKey, segmentIndex);
    const PathExecutionPlan *pathPlan = findPathPlan(executionPathKey);
    if (!pathState || !segmentState || !timeline || !pathPlan)
    {
        outcome.errorMessage = QStringLiteral(
            "Completion references an unknown path or segment");
        return outcome;
    }

    segmentState->lifecycle = SegmentLifecycleState::Completed;
    recordLatestProgressTime(timeline->completedAtSeconds,
                             eventTimeSeconds);

    if (segmentIndex + 1 < pathPlan->segments.size())
    {
        pathState->lifecycle = PathLifecycleState::ReadyForNextSegment;
        pathState->activeSegmentIndex = segmentIndex + 1;
    }
    else
    {
        pathState->lifecycle = PathLifecycleState::Completed;
        pathState->activeSegmentIndex = segmentIndex;
    }

    m_dispatchableSegments = recomputeDispatchableSegments();
    outcome.accepted = true;
    outcome.advancedState = true;
    outcome.newlyDispatchableSegments = m_dispatchableSegments;
    return outcome;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
