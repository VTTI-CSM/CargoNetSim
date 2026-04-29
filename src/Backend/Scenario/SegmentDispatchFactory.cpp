#include "SegmentDispatchFactory.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Scenario/ExecutionContainerIdentity.h"
#include "Backend/Scenario/RuntimeArtifactIdentity.h"

#include <QVector>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QString summarizeContainerIds(
    const QList<ContainerCore::Container *> &containers)
{
    QStringList ids;
    ids.reserve(qMin(containers.size(), 3));
    for (int i = 0; i < containers.size() && i < 3; ++i)
    {
        if (containers[i])
            ids.append(containers[i]->getContainerID());
    }
    if (containers.size() > 3)
        ids.append(QStringLiteral("..."));
    return ids.join(QStringLiteral(", "));
}

struct TerminalHandlingMetadata
{
    QString executionId;
    QString pathIdentity;
    QString canonicalPathKey;
    QString scenarioTerminalId;
    QString runtimeTerminalId;
    int     terminalSequenceIndex = -1;
    int     segmentIndex = -1;
    QString vehicleMode;
    QString vehicleId;
};

int numVehiclesNeeded(int containerCount, int perVehicle)
{
    if (perVehicle <= 0)
        return 0;
    return qMax(1, (containerCount + perVehicle - 1) / perVehicle);
}

struct VehicleContainerSelection
{
    QList<ContainerCore::Container *> dispatchContainers;
    QStringList                       logicalContainerIds;
};

QString logicalContainerIdFor(
    const ContainerCore::Container *container)
{
    if (!container)
        return {};
    return ExecutionContainers::logicalContainerIdFor(*container);
}

VehicleContainerSelection takeContainersForVehicle(
    int                                   take,
    QList<ContainerCore::Container *>    &source,
    int                                   segmentIndex,
    int                                  &containerCounter,
    const QString                        &currentLocation,
    const QString                        &destination,
    const TerminalHandlingMetadata       &metadata)
{
    VehicleContainerSelection selection;
    for (int j = 0; j < take && !source.isEmpty(); ++j)
    {
        auto *orig = source.takeFirst();
        const QString logicalId = logicalContainerIdFor(orig);
        auto *copy = orig->copy();
        copy->setContainerID(
            RuntimeArtifacts::copiedContainerId(
                metadata.canonicalPathKey, segmentIndex,
                containerCounter++,
                orig->getContainerID()));
        ExecutionContainerMetadata executionMetadata;
        executionMetadata.executionId = metadata.executionId;
        executionMetadata.pathIdentity =
            metadata.pathIdentity.isEmpty()
                ? metadata.canonicalPathKey
                : metadata.pathIdentity;
        executionMetadata.canonicalPathKey = metadata.canonicalPathKey;
        executionMetadata.sourceContainerId =
            ExecutionContainers::sourceContainerIdFor(*orig);
        executionMetadata.executionContainerId = logicalId;
        executionMetadata.scenarioTerminalId =
            metadata.scenarioTerminalId;
        executionMetadata.runtimeTerminalId =
            metadata.runtimeTerminalId;
        executionMetadata.readySegmentIndex =
            metadata.segmentIndex + 1;
        executionMetadata.terminalSequenceIndex =
            metadata.terminalSequenceIndex;
        executionMetadata.segmentIndex = metadata.segmentIndex;
        executionMetadata.vehicleMode = metadata.vehicleMode;
        executionMetadata.vehicleId = metadata.vehicleId;
        ExecutionContainers::applyDispatchMetadata(
            *copy, executionMetadata);
        copy->setContainerCurrentLocation(currentLocation);
        ExecutionContainers::replaceNextDestination(*copy,
                                                    destination);
        selection.dispatchContainers.append(copy);
        selection.logicalContainerIds.append(logicalId);
    }
    return selection;
}

} // namespace

SegmentDispatchFactory::SegmentDispatchFactory(
    VehicleController *vehicles, const QString &executionId)
    : m_vehicles(vehicles)
    , m_executionId(executionId)
{
}

bool SegmentDispatchFactory::appendTrainSegment(
    const TrainSegmentDispatchRequest &request,
    SegmentDispatchCursor            &cursor,
    SimulationRequestBundle         &bundle,
    QVector<VehicleLoadManifest>    *vehicleLoads,
    QString                         *err) const
{
    if (request.pathId < 0)
    {
        if (err) *err = QStringLiteral("Train segment dispatch requires a valid path ID");
        return false;
    }
    if (request.canonicalPathKey.isEmpty())
    {
        if (err) *err = QStringLiteral("Train segment dispatch requires a canonical path key");
        return false;
    }
    if (!request.network)
    {
        if (err) *err = QStringLiteral("Train segment dispatch requires a resolved rail network");
        return false;
    }
    if (!m_vehicles)
    {
        if (err) *err = QStringLiteral("Train segment dispatch requires a vehicle controller");
        return false;
    }

    bundle.trainNetworks[request.networkName] = request.network;

    QList<ContainerCore::Container *> containersCopy =
        request.containersAvailable;
    if (containersCopy.isEmpty())
    {
        qCWarning(lcScenario)
            << "SegmentDispatchFactory::appendTrainSegment:"
            << "no containers available for train segment, but vehicle scheduling will still apply min-one rule"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "network=" << request.networkName;
    }

    const int numTrains = numVehiclesNeeded(
        containersCopy.size(), request.trainContainerCapacity);
    qCDebug(lcScenario)
        << "SegmentDispatchFactory::appendTrainSegment:"
        << "containersAvailable=" << containersCopy.size()
        << "trainCapacity=" << request.trainContainerCapacity
        << "numTrains=" << numTrains;

    for (int i = 0; i < numTrains; ++i)
    {
        const QString trainId =
            RuntimeArtifacts::vehicleId(
                request.canonicalPathKey, request.segmentIndex,
                cursor.trainCounter++,
                QStringLiteral("train"));

        auto *baseTrain = m_vehicles->getRandomTrain();
        if (!baseTrain)
        {
            if (err)
                *err = QStringLiteral(
                    "No train available for segment dispatch");
            return false;
        }
        auto *train = baseTrain->copy();
        train->setUserId(trainId);

        QVector<int> trainPath;
        trainPath << request.runtimeStartNodeId << request.runtimeEndNodeId;
        train->setTrainPathOnNodeIds(trainPath);
        train->setLoadTime(static_cast<float>(cursor.trainCounter * 100));

        TrainSimData td;
        td.train = train;
        const TerminalHandlingMetadata metadata{
            m_executionId,
            request.pathIdentity,
            request.canonicalPathKey,
            request.endTerminalId,
            QString(),
            request.segmentIndex + 1,
            request.segmentIndex,
            QStringLiteral("Train"),
            trainId };
        const auto selection = takeContainersForVehicle(
            qMin(request.trainContainerCapacity, containersCopy.size()),
            containersCopy, request.segmentIndex,
            cursor.containerCounter,
            QString::number(request.runtimeStartNodeId),
            QString::number(request.runtimeEndNodeId), metadata);
        td.containers = selection.dispatchContainers;
        qCInfo(lcScenario)
            << "SegmentDispatchFactory::appendTrainSegment:"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "trainId=" << trainId
            << "network=" << request.networkName
            << "scenarioStart=" << request.startTerminalId
            << "scenarioEnd=" << request.endTerminalId
            << "runtimeStartNode=" << request.runtimeStartNodeId
            << "runtimeEndNode=" << request.runtimeEndNodeId
            << "containerCount=" << td.containers.size()
            << "containerIds=[" << summarizeContainerIds(td.containers)
            << "]";
        bundle.trainData[request.networkName].append(td);
        if (vehicleLoads)
        {
            vehicleLoads->append(
                { trainId, request.networkName,
                  selection.logicalContainerIds });
        }
    }

    if (err)
        err->clear();
    return true;
}

bool SegmentDispatchFactory::appendTruckSegment(
    const TruckSegmentDispatchRequest &request,
    SegmentDispatchCursor            &cursor,
    SimulationRequestBundle         &bundle,
    QVector<VehicleLoadManifest>    *vehicleLoads,
    QString                         *err) const
{
    if (request.pathId < 0)
    {
        if (err) *err = QStringLiteral("Truck segment dispatch requires a valid path ID");
        return false;
    }
    if (request.canonicalPathKey.isEmpty())
    {
        if (err) *err = QStringLiteral("Truck segment dispatch requires a canonical path key");
        return false;
    }
    if (!request.network)
    {
        if (err) *err = QStringLiteral("Truck segment dispatch requires a resolved truck network");
        return false;
    }

    bundle.truckNetworks[request.networkName] = request.network;

    QList<ContainerCore::Container *> containersCopy =
        request.containersAvailable;
    if (containersCopy.isEmpty())
    {
        qCWarning(lcScenario)
            << "SegmentDispatchFactory::appendTruckSegment:"
            << "no containers available for truck segment, but vehicle scheduling will still apply min-one rule"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "network=" << request.networkName;
    }

    const int numTrucks = numVehiclesNeeded(
        containersCopy.size(), request.truckContainerCapacity);
    qCDebug(lcScenario)
        << "SegmentDispatchFactory::appendTruckSegment:"
        << "containersAvailable=" << containersCopy.size()
        << "truckCapacity=" << request.truckContainerCapacity
        << "numTrucks=" << numTrucks;

    for (int i = 0; i < numTrucks; ++i)
    {
        TruckSimData td;
        td.tripId = RuntimeArtifacts::vehicleId(
            request.canonicalPathKey, request.segmentIndex,
            cursor.truckCounter++,
            QStringLiteral("truck"));
        td.originNode = request.runtimeStartNodeId;
        td.destinationNode = request.runtimeEndNodeId;
        const TerminalHandlingMetadata metadata{
            m_executionId,
            request.pathIdentity,
            request.canonicalPathKey,
            request.endTerminalId,
            QString(),
            request.segmentIndex + 1,
            request.segmentIndex,
            QStringLiteral("Truck"),
            td.tripId };
        const auto selection = takeContainersForVehicle(
            qMin(request.truckContainerCapacity, containersCopy.size()),
            containersCopy, request.segmentIndex,
            cursor.containerCounter,
            request.runtimeStartLocationName,
            request.runtimeEndLocationName,
            metadata);
        td.containers = selection.dispatchContainers;
        qCInfo(lcScenario)
            << "SegmentDispatchFactory::appendTruckSegment:"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "tripId=" << td.tripId
            << "network=" << request.networkName
            << "scenarioStart=" << request.startTerminalId
            << "scenarioEnd=" << request.endTerminalId
            << "runtimeStartNode=" << request.runtimeStartNodeId
            << "runtimeEndNode=" << request.runtimeEndNodeId
            << "runtimeStartLocation=" << request.runtimeStartLocationName
            << "runtimeEndLocation=" << request.runtimeEndLocationName
            << "containerCount=" << td.containers.size()
            << "containerIds=[" << summarizeContainerIds(td.containers)
            << "]";
        bundle.truckData[request.networkName].append(td);
        if (vehicleLoads)
        {
            vehicleLoads->append(
                { td.tripId, request.networkName,
                  selection.logicalContainerIds });
        }
    }

    if (err)
        err->clear();
    return true;
}

bool SegmentDispatchFactory::appendShipSegment(
    const ShipSegmentDispatchRequest &request,
    SegmentDispatchCursor           &cursor,
    SimulationRequestBundle        &bundle,
    QVector<VehicleLoadManifest>   *vehicleLoads,
    QString                        *err) const
{
    if (request.pathId < 0)
    {
        if (err) *err = QStringLiteral("Ship segment dispatch requires a valid path ID");
        return false;
    }
    if (request.canonicalPathKey.isEmpty())
    {
        if (err) *err = QStringLiteral("Ship segment dispatch requires a canonical path key");
        return false;
    }
    if (!m_vehicles)
    {
        if (err) *err = QStringLiteral("Ship segment dispatch requires a vehicle controller");
        return false;
    }

    QList<ContainerCore::Container *> containersCopy =
        request.containersAvailable;
    if (containersCopy.isEmpty())
    {
        qCWarning(lcScenario)
            << "SegmentDispatchFactory::appendShipSegment:"
            << "no containers available for ship segment, but vehicle scheduling will still apply min-one rule"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "network=" << request.networkName;
    }

    const int numShips = numVehiclesNeeded(
        containersCopy.size(), request.shipContainerCapacity);
    qCDebug(lcScenario)
        << "SegmentDispatchFactory::appendShipSegment:"
        << "containersAvailable=" << containersCopy.size()
        << "shipCapacity=" << request.shipContainerCapacity
        << "numShips=" << numShips;

    for (int i = 0; i < numShips; ++i)
    {
        const QString shipId =
            RuntimeArtifacts::vehicleId(
                request.canonicalPathKey, request.segmentIndex,
                cursor.shipCounter++,
                QStringLiteral("ship"));

        auto *baseShip = m_vehicles->getRandomShip();
        if (!baseShip)
        {
            if (err)
                *err = QStringLiteral(
                    "No ship available for segment dispatch");
            return false;
        }
        auto *ship = baseShip->copy();
        ship->setUserId(shipId);

        QVector<QPointF> shipPath;
        shipPath << request.startGlobalPosition << request.endGlobalPosition;
        ship->setPathCoordinates(shipPath);

        ShipSimData sd;
        sd.ship = ship;
        sd.destinationTerminal = request.endTerminalId;
        const TerminalHandlingMetadata metadata{
            m_executionId,
            request.pathIdentity,
            request.canonicalPathKey,
            request.endTerminalId,
            QString(),
            request.segmentIndex + 1,
            request.segmentIndex,
            QStringLiteral("Ship"),
            shipId };
        const auto selection = takeContainersForVehicle(
            qMin(request.shipContainerCapacity, containersCopy.size()),
            containersCopy, request.segmentIndex,
            cursor.containerCounter,
            request.startTerminalId, request.endTerminalId, metadata);
        sd.containers = selection.dispatchContainers;
        qCInfo(lcScenario)
            << "SegmentDispatchFactory::appendShipSegment:"
            << "pathId=" << request.pathId
            << "pathKey=" << request.canonicalPathKey
            << "segment=" << request.segmentIndex
            << "shipId=" << shipId
            << "network=" << request.networkName
            << "scenarioStart=" << request.startTerminalId
            << "scenarioEnd=" << request.endTerminalId
            << "destinationTerminal=" << sd.destinationTerminal
            << "containerCount=" << sd.containers.size()
            << "containerIds=[" << summarizeContainerIds(sd.containers)
            << "]";
        bundle.shipData[request.networkName].append(sd);
        if (vehicleLoads)
        {
            vehicleLoads->append(
                { shipId, request.networkName,
                  selection.logicalContainerIds });
        }
    }

    if (err)
        err->clear();
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
