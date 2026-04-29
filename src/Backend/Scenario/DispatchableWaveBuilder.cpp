#include "DispatchableWaveBuilder.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "PropertyKeys.h"

#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

bool isDispatchableCustody(
    ContainerCustodyState custody)
{
    return custody == ContainerCustodyState::AtOrigin
        || custody == ContainerCustodyState::ReadyForPickup;
}

} // namespace

DispatchableWaveBuilder::DispatchableWaveBuilder(
    const ScenarioRegistry &registry, ConfigController *config,
    VehicleController *vehicles, const QString &executionId,
    TerminalPickupCoordinator *pickupCoordinator)
    : m_registry(registry)
    , m_config(config)
    , m_dispatchFactory(vehicles, executionId)
    , m_pickupCoordinator(pickupCoordinator)
{
}

DispatchableWaveBuildResult DispatchableWaveBuilder::build(
    const ScenarioExecutionPlan &plan,
    const ExecutionLedger &ledger,
    const QVector<DispatchableSegmentRef> &dispatchableSegments) const
{
    if (!m_config)
    {
        return fail(QStringLiteral(
            "DispatchableWaveBuilder requires a config controller"));
    }
    if (!m_pickupCoordinator)
    {
        return fail(QStringLiteral(
            "DispatchableWaveBuilder requires a terminal pickup coordinator"));
    }

    DispatchableWaveBuildResult result;
    SegmentDispatchCursor cursor;
    auto releaseReservedPickups = [&]() {
        if (!m_pickupCoordinator
            || result.pickupReservations.isEmpty())
        {
            return;
        }
        QString releaseError;
        m_pickupCoordinator->releaseReservations(
            result.pickupReservations, &releaseError);
        if (!releaseError.isEmpty())
        {
            qCWarning(lcScenario)
                << "DispatchableWaveBuilder::build:"
                << "failed to release terminal pickup reservations after build failure:"
                << releaseError;
        }
    };
    auto failAfterReservations =
        [&](const QString &message) {
            releaseReservedPickups();
            return fail(message);
        };
    auto blockedAfterReservations =
        [&](const PathExecutionPlan &pathPlan,
            const SegmentExecutionPlan &segmentPlan) {
            releaseReservedPickups();
            DispatchableWaveBuildResult blocked;
            blocked.terminalBlocked = true;
            blocked.terminalBlockedMessage =
                QStringLiteral(
                    "Terminal %1 has not released all containers for path %2 segment %3")
                    .arg(segmentPlan.startTerminalId,
                         pathPlan.pathIdentity)
                    .arg(segmentPlan.segmentIndex);
            return blocked;
        };

    for (const auto &segmentRef : dispatchableSegments)
    {
        const auto *pathPlan =
            findPathPlan(plan, segmentRef.pathIdentity);
        if (!pathPlan)
        {
            return fail(QStringLiteral(
                "Unknown path identity in dispatchable wave: %1")
                            .arg(segmentRef.pathIdentity));
        }

        if (segmentRef.segmentIndex < 0
            || segmentRef.segmentIndex >= pathPlan->segments.size())
        {
            return fail(QStringLiteral(
                "Invalid dispatchable segment index %1 for %2")
                            .arg(segmentRef.segmentIndex)
                            .arg(segmentRef.pathIdentity));
        }

        const auto &segmentPlan =
            pathPlan->segments[segmentRef.segmentIndex];
        const auto stateIt =
            ledger.containerStates.constFind(pathPlan->pathIdentity);
        if (stateIt == ledger.containerStates.constEnd())
        {
            return fail(QStringLiteral(
                "Missing container ledger state for %1")
                            .arg(pathPlan->pathIdentity));
        }

        int eligibleContainerCount = 0;
        QList<ContainerCore::Container *> eligibleContainers;
        for (const auto &containerState : stateIt.value())
        {
            if (containerState.segmentIndex
                    != segmentRef.segmentIndex
                || containerState.currentTerminalId
                       != segmentPlan.startTerminalId
                || !isDispatchableCustody(
                    containerState.custody))
            {
                continue;
            }

            ++eligibleContainerCount;
        }

        if (eligibleContainerCount <= 0)
        {
            return failAfterReservations(QStringLiteral(
                "No dispatchable containers are staged for path %1 segment %2")
                            .arg(pathPlan->pathIdentity)
                            .arg(segmentRef.segmentIndex));
        }

        {
            TerminalPickupRequest pickupRequest;
            pickupRequest.executionId = plan.executionId;
            pickupRequest.pathIdentity = pathPlan->pathIdentity;
            pickupRequest.canonicalPathKey =
                pathPlan->canonicalPathKey;
            pickupRequest.terminalId = segmentPlan.startTerminalId;
            pickupRequest.segmentIndex = segmentRef.segmentIndex;
            pickupRequest.containerCount = eligibleContainerCount;

            auto pickupBatch =
                m_pickupCoordinator->reserveForDispatch(
                    pickupRequest);
            if (pickupBatch.blocked)
            {
                return blockedAfterReservations(*pathPlan,
                                                segmentPlan);
            }
            if (!pickupBatch.isSuccess())
            {
                return failAfterReservations(
                    pickupBatch.errorMessage);
            }
            result.pickupReservations.append(pickupBatch.handle);
            eligibleContainers = pickupBatch.containers;
        }

        QVector<VehicleLoadManifest> loads;
        QString err;
        bool success = false;
        const int capacity =
            capacityForMode(segmentPlan.mode);
        if (capacity <= 0)
        {
            qDeleteAll(eligibleContainers);
            return failAfterReservations(QStringLiteral(
                "Invalid configured vehicle capacity for mode %1")
                            .arg(transportationModeToString(
                                segmentPlan.mode)));
        }

        switch (segmentPlan.mode)
        {
        case TransportationTypes::TransportationMode::Train:
        {
            auto *network = NetworkLookup::findRail(
                m_registry, segmentPlan.regionName,
                segmentPlan.networkName);
            TrainSegmentDispatchRequest request;
            request.pathId = pathPlan->pathId;
            request.pathIdentity = pathPlan->pathIdentity;
            request.canonicalPathKey =
                pathPlan->canonicalPathKey;
            request.segmentIndex = segmentPlan.segmentIndex;
            request.startTerminalId =
                segmentPlan.startTerminalId;
            request.endTerminalId =
                segmentPlan.endTerminalId;
            request.networkName = segmentPlan.networkName;
            request.runtimeStartNodeId =
                segmentPlan.runtimeStartNodeId;
            request.runtimeEndNodeId =
                segmentPlan.runtimeEndNodeId;
            request.trainContainerCapacity = capacity;
            request.containersAvailable = eligibleContainers;
            request.network = network;
            success = m_dispatchFactory.appendTrainSegment(
                request, cursor, result.bundle, &loads, &err);
            break;
        }
        case TransportationTypes::TransportationMode::Truck:
        {
            auto *network = NetworkLookup::findTruck(
                m_registry, segmentPlan.regionName,
                segmentPlan.networkName);
            TruckSegmentDispatchRequest request;
            request.pathId = pathPlan->pathId;
            request.pathIdentity = pathPlan->pathIdentity;
            request.canonicalPathKey =
                pathPlan->canonicalPathKey;
            request.segmentIndex = segmentPlan.segmentIndex;
            request.startTerminalId =
                segmentPlan.startTerminalId;
            request.endTerminalId =
                segmentPlan.endTerminalId;
            request.networkName = segmentPlan.networkName;
            request.runtimeStartNodeId =
                segmentPlan.runtimeStartNodeId;
            request.runtimeEndNodeId =
                segmentPlan.runtimeEndNodeId;
            request.runtimeStartLocationName =
                segmentPlan.runtimeStartLocationName;
            request.runtimeEndLocationName =
                segmentPlan.runtimeEndLocationName;
            request.truckContainerCapacity = capacity;
            request.containersAvailable = eligibleContainers;
            request.network = network;
            success = m_dispatchFactory.appendTruckSegment(
                request, cursor, result.bundle, &loads, &err);
            break;
        }
        case TransportationTypes::TransportationMode::Ship:
        {
            ShipSegmentDispatchRequest request;
            request.pathId = pathPlan->pathId;
            request.pathIdentity = pathPlan->pathIdentity;
            request.canonicalPathKey =
                pathPlan->canonicalPathKey;
            request.segmentIndex = segmentPlan.segmentIndex;
            request.startTerminalId =
                segmentPlan.startTerminalId;
            request.endTerminalId =
                segmentPlan.endTerminalId;
            request.networkName = segmentPlan.networkName;
            request.startGlobalPosition =
                segmentPlan.startGlobalPosition;
            request.endGlobalPosition =
                segmentPlan.endGlobalPosition;
            request.shipContainerCapacity = capacity;
            request.containersAvailable = eligibleContainers;
            success = m_dispatchFactory.appendShipSegment(
                request, cursor, result.bundle, &loads, &err);
            break;
        }
        default:
            err = QStringLiteral(
                "Unsupported dispatch mode in execution plan");
            success = false;
            break;
        }

        qDeleteAll(eligibleContainers);

        if (!success)
            return failAfterReservations(err);

        for (const auto &load : loads)
        {
            VehicleDispatchAssignment assignment;
            assignment.pathIdentity = pathPlan->pathIdentity;
            assignment.canonicalPathKey =
                pathPlan->canonicalPathKey;
            assignment.pathId = pathPlan->pathId;
            assignment.segmentIndex = segmentPlan.segmentIndex;
            assignment.vehicleId = load.vehicleId;
            assignment.networkName = load.networkName;
            assignment.startTerminalId =
                segmentPlan.startTerminalId;
            assignment.endTerminalId =
                segmentPlan.endTerminalId;
            assignment.logicalContainerIds =
                load.logicalContainerIds;
            result.assignments.append(assignment);
        }
    }

    return result;
}

int DispatchableWaveBuilder::capacityForMode(
    TransportationTypes::TransportationMode mode) const
{
    const QVariantMap transportModes =
        m_config->getTransportModes();
    switch (mode)
    {
    case TransportationTypes::TransportationMode::Train:
        return transportModes.value(
                   QStringLiteral("rail"))
            .toMap()
            .value(PropertyKeys::Mode::AverageContainerNumber,
                   -1)
            .toInt();
    case TransportationTypes::TransportationMode::Truck:
        return transportModes.value(
                   QStringLiteral("truck"))
            .toMap()
            .value(PropertyKeys::Mode::AverageContainerNumber,
                   -1)
            .toInt();
    case TransportationTypes::TransportationMode::Ship:
        return transportModes.value(
                   QStringLiteral("ship"))
            .toMap()
            .value(PropertyKeys::Mode::AverageContainerNumber,
                   -1)
            .toInt();
    default:
        return -1;
    }
}

const PathExecutionPlan *DispatchableWaveBuilder::findPathPlan(
    const ScenarioExecutionPlan &plan,
    const QString &pathIdentity) const
{
    return plan.findPath(pathIdentity);
}

DispatchableWaveBuildResult DispatchableWaveBuilder::fail(
    const QString &message) const
{
    DispatchableWaveBuildResult result;
    result.errorMessage = message;
    return result;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
