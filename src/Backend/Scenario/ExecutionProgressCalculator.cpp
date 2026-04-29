#include "ExecutionProgressCalculator.h"

#include <QtGlobal>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

double clampPercent(double value)
{
    return qBound(0.0, value, 100.0);
}

double vehicleLifecycleProgress(VehicleLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case VehicleLifecycleState::Dispatched:
        return 10.0;
    case VehicleLifecycleState::VehicleArrived:
        return 55.0;
    case VehicleLifecycleState::UnloadCompleted:
        return 75.0;
    case VehicleLifecycleState::TerminalProcessing:
        return 90.0;
    case VehicleLifecycleState::Completed:
        return 100.0;
    case VehicleLifecycleState::Failed:
        return 0.0;
    }

    return 0.0;
}

double lifecycleOnlySegmentProgress(SegmentLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case SegmentLifecycleState::Pending:
        return 0.0;
    case SegmentLifecycleState::Dispatched:
    case SegmentLifecycleState::VehicleRunning:
        return 10.0;
    case SegmentLifecycleState::VehicleArrived:
        return 55.0;
    case SegmentLifecycleState::UnloadCompleted:
    case SegmentLifecycleState::TerminalHandoffCompleted:
        return 75.0;
    case SegmentLifecycleState::TerminalProcessing:
        return 90.0;
    case SegmentLifecycleState::ReadyForPickup:
    case SegmentLifecycleState::Completed:
    case SegmentLifecycleState::Skipped:
        return 100.0;
    case SegmentLifecycleState::Failed:
        return 0.0;
    }

    return 0.0;
}

double segmentProgress(
    const SegmentExecutionState *segmentState)
{
    if (!segmentState)
        return 0.0;

    if (segmentState->vehicleStates.isEmpty())
    {
        return lifecycleOnlySegmentProgress(
            segmentState->lifecycle);
    }

    double total = 0.0;
    for (auto it = segmentState->vehicleStates.constBegin();
         it != segmentState->vehicleStates.constEnd(); ++it)
    {
        total += vehicleLifecycleProgress(it.value().lifecycle);
    }

    return clampPercent(total / segmentState->vehicleStates.size());
}

bool isActiveSegmentLifecycle(SegmentLifecycleState lifecycle)
{
    return lifecycle == SegmentLifecycleState::Dispatched
        || lifecycle == SegmentLifecycleState::VehicleRunning
        || lifecycle == SegmentLifecycleState::VehicleArrived
        || lifecycle == SegmentLifecycleState::UnloadCompleted
        || lifecycle == SegmentLifecycleState::TerminalHandoffCompleted
        || lifecycle == SegmentLifecycleState::TerminalProcessing
        || lifecycle == SegmentLifecycleState::ReadyForPickup;
}

const SegmentExecutionState *segmentStateAt(
    const ExecutionLedger &ledger,
    const QString         &pathIdentity,
    int                    segmentIndex)
{
    const auto statesIt =
        ledger.segmentStates.constFind(pathIdentity);
    if (statesIt == ledger.segmentStates.constEnd()
        || segmentIndex < 0
        || segmentIndex >= statesIt.value().size())
    {
        return nullptr;
    }

    return &statesIt.value().at(segmentIndex);
}

PathLifecycleState pathLifecycleFor(
    const PathExecutionPlan &pathPlan,
    const ExecutionLedger   &ledger)
{
    const auto it =
        ledger.pathStates.constFind(pathPlan.pathIdentity);
    if (it != ledger.pathStates.constEnd())
        return it.value().lifecycle;

    return pathPlan.isExecutable() ? PathLifecycleState::Pending
                                   : PathLifecycleState::Skipped;
}

int activeSegmentIndexFor(
    const PathExecutionPlan &pathPlan,
    const ExecutionLedger   &ledger)
{
    const auto pathStateIt =
        ledger.pathStates.constFind(pathPlan.pathIdentity);
    if (pathStateIt != ledger.pathStates.constEnd()
        && pathStateIt.value().activeSegmentIndex >= 0)
    {
        return pathStateIt.value().activeSegmentIndex;
    }

    const auto segmentStatesIt =
        ledger.segmentStates.constFind(pathPlan.pathIdentity);
    if (segmentStatesIt != ledger.segmentStates.constEnd())
    {
        const auto &segmentStates = segmentStatesIt.value();
        for (int i = 0; i < segmentStates.size(); ++i)
        {
            if (isActiveSegmentLifecycle(
                    segmentStates.at(i).lifecycle))
            {
                return i;
            }
        }
    }

    return pathPlan.segments.isEmpty() ? -1 : 0;
}

} // namespace

ExecutionProgressSnapshot calculateExecutionProgress(
    const ScenarioExecutionPlan &plan,
    const ExecutionLedger       &ledger)
{
    ExecutionProgressSnapshot snapshot;

    double weightedProgressSum = 0.0;

    for (const auto &pathPlan : plan.paths)
    {
        PathProgressSnapshot pathSnapshot;
        pathSnapshot.pathIdentity = pathPlan.pathIdentity;
        pathSnapshot.canonicalPathKey = pathPlan.canonicalPathKey;
        pathSnapshot.pathId = pathPlan.pathId;
        pathSnapshot.rank = pathPlan.rank;
        pathSnapshot.originId = pathPlan.originId;
        pathSnapshot.destinationId = pathPlan.destinationId;
        pathSnapshot.disposition = pathPlan.disposition;
        pathSnapshot.lifecycle =
            pathLifecycleFor(pathPlan, ledger);
        pathSnapshot.executable = pathPlan.isExecutable();
        pathSnapshot.message = pathPlan.planningMessage;
        pathSnapshot.totalSegments = pathPlan.segments.size();
        pathSnapshot.activeSegmentIndex =
            activeSegmentIndexFor(pathPlan, ledger);

        double pathProgressSum = 0.0;

        for (const auto &segmentPlan : pathPlan.segments)
        {
            const SegmentExecutionState *segmentState =
                segmentStateAt(ledger, pathPlan.pathIdentity,
                               segmentPlan.segmentIndex);

            SegmentProgressSnapshot segmentSnapshot;
            segmentSnapshot.segmentIndex =
                segmentPlan.segmentIndex;
            segmentSnapshot.segmentId = segmentPlan.segmentId;
            segmentSnapshot.mode = segmentPlan.mode;
            segmentSnapshot.networkName = segmentPlan.networkName;
            segmentSnapshot.startTerminalId =
                segmentPlan.startTerminalId;
            segmentSnapshot.endTerminalId =
                segmentPlan.endTerminalId;
            segmentSnapshot.lifecycle = segmentState
                ? segmentState->lifecycle
                : SegmentLifecycleState::Pending;
            segmentSnapshot.dispatchedVehicleCount = segmentState
                ? segmentState->dispatchedVehicleCount()
                : 0;
            segmentSnapshot.completedVehicleCount = segmentState
                ? segmentState->completedVehicleCount()
                : 0;
            segmentSnapshot.percent =
                segmentProgress(segmentState);
            segmentSnapshot.active =
                segmentPlan.segmentIndex
                    == pathSnapshot.activeSegmentIndex
                && isActiveSegmentLifecycle(
                    segmentSnapshot.lifecycle);

            if (segmentSnapshot.lifecycle
                == SegmentLifecycleState::Completed)
            {
                ++pathSnapshot.completedSegments;
            }

            if (segmentPlan.segmentIndex
                == pathSnapshot.activeSegmentIndex)
            {
                pathSnapshot.activeMode = segmentPlan.mode;
                pathSnapshot.activeNetworkName =
                    segmentPlan.networkName;
                pathSnapshot.activeStartTerminalId =
                    segmentPlan.startTerminalId;
                pathSnapshot.activeEndTerminalId =
                    segmentPlan.endTerminalId;
            }

            pathProgressSum += segmentSnapshot.percent;
            pathSnapshot.segments.append(segmentSnapshot);
        }

        if (pathSnapshot.executable)
        {
            ++snapshot.executablePathCount;
            snapshot.executableSegmentCount +=
                pathSnapshot.totalSegments;
            snapshot.completedExecutableSegments +=
                pathSnapshot.completedSegments;

            if (pathSnapshot.lifecycle
                == PathLifecycleState::Completed)
            {
                ++snapshot.completedExecutablePaths;
            }

            if (pathSnapshot.totalSegments > 0)
            {
                pathSnapshot.percent = clampPercent(
                    pathProgressSum / pathSnapshot.totalSegments);
                weightedProgressSum += pathProgressSum;
            }
        }
        else
        {
            pathSnapshot.percent =
                pathSnapshot.lifecycle
                        == PathLifecycleState::Skipped
                    ? 100.0
                    : 0.0;
        }

        snapshot.paths.append(pathSnapshot);
    }

    if (snapshot.executableSegmentCount > 0)
    {
        snapshot.aggregatePercent = clampPercent(
            weightedProgressSum
            / snapshot.executableSegmentCount);
    }

    if (snapshot.executablePathCount > 0
        && snapshot.completedExecutablePaths
               == snapshot.executablePathCount)
    {
        snapshot.aggregatePercent = 100.0;
    }

    return snapshot;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim

