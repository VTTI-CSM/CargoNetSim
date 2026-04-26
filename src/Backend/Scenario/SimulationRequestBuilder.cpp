#include "SimulationRequestBuilder.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RuntimeArtifactIdentity.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "PropertyKeys.h"

#include <QVector>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

void SimulationRequestBundle::merge(const SimulationRequestBundle &other)
{
    for (auto it = other.shipData.constBegin();
         it != other.shipData.constEnd(); ++it)
        shipData[it.key()].append(it.value());
    for (auto it = other.trainData.constBegin();
         it != other.trainData.constEnd(); ++it)
        trainData[it.key()].append(it.value());
    for (auto it = other.truckData.constBegin();
         it != other.truckData.constEnd(); ++it)
        truckData[it.key()].append(it.value());
    for (auto it = other.trainNetworks.constBegin();
         it != other.trainNetworks.constEnd(); ++it)
        trainNetworks.insert(it.key(), it.value());
    for (auto it = other.truckNetworks.constBegin();
         it != other.truckNetworks.constEnd(); ++it)
        truckNetworks.insert(it.key(), it.value());
}

SimulationRequestBuilder::SimulationRequestBuilder(
    const ScenarioDocument &doc,
    const ScenarioRegistry &registry,
    ConfigController       *config,
    RegionDataController   *regionData,
    VehicleController      *vehicles,
    const QString          &executionId)
    : m_doc(doc)
    , m_registry(registry)
    , m_config(config)
    , m_regionData(regionData)
    , m_vehicles(vehicles)
    , m_executionId(executionId)
{
}

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

struct LinkagePair
{
    NodeLinkage start;
    NodeLinkage end;
};

struct TerminalHandlingMetadata
{
    QString executionId;
    QString pathIdentity;
    QString scenarioTerminalId;
    QString runtimeTerminalId;
    int     terminalSequenceIndex = -1;
    int     segmentIndex = -1;
    QString vehicleMode;
    QString vehicleId;
};

// linkagesFor() already filters by NetworkSpec::Type, so this helper
// matches only on networkName. NodeLinkage has no networkType field —
// the type is resolved by the caller via region-network lookup.
QList<LinkagePair> intersectLinkagesByNetwork(
    const QList<NodeLinkage> &a,
    const QList<NodeLinkage> &b)
{
    QList<LinkagePair> result;
    for (const auto &la : a)
    {
        for (const auto &lb : b)
        {
            if (la.networkName == lb.networkName
                && la.nodeId    != lb.nodeId)
            {
                result.append({la, lb});
            }
        }
    }
    return result;
}

// Ceil-divide container count by per-vehicle capacity, with min of 1
// so a single container still books one vehicle. Shared by both train
// and truck segment builders.
int numVehiclesNeeded(int containerCount, int perVehicle)
{
    if (perVehicle <= 0)
        return 0;
    return qMax(1, (containerCount + perVehicle - 1) / perVehicle);
}

// Take up to `take` containers from `source` (mutates it), deep-copy each,
// rewrite IDs as "<pathId>_<originId>_<counter>", and set current-location
// and destination strings. Shared by both train and truck segments.
QList<ContainerCore::Container *> takeContainersForVehicle(
    int                                   take,
    QList<ContainerCore::Container *>    &source,
    const CargoNetSim::Backend::Path     *path,
    int                                   segmentIndex,
    int                                  &containerCounter,
    const QString                        &currentLocation,
    const QString                        &destination,
    const TerminalHandlingMetadata       &metadata)
{
    QList<ContainerCore::Container *> result;
    using Hauler = ContainerCore::Container::HaulerType;
    for (int j = 0; j < take && !source.isEmpty(); ++j)
    {
        auto *orig = source.takeFirst();
        auto *copy = orig->copy();
        copy->setContainerID(
            RuntimeArtifacts::copiedContainerId(
                path, segmentIndex, containerCounter++,
                orig->getContainerID()));
        if (!metadata.executionId.isEmpty())
        {
            copy->addCustomVariable(
                Hauler::noHauler,
                QStringLiteral("execution_id"),
                metadata.executionId);
        }
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("path_identity"),
            metadata.pathIdentity);
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("scenario_terminal_id"),
            metadata.scenarioTerminalId);
        if (!metadata.runtimeTerminalId.isEmpty())
        {
            copy->addCustomVariable(
                Hauler::noHauler,
                QStringLiteral("runtime_terminal_id"),
                metadata.runtimeTerminalId);
        }
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("terminal_sequence_index"),
            metadata.terminalSequenceIndex);
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("segment_index"),
            metadata.segmentIndex);
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("vehicle_mode"),
            metadata.vehicleMode);
        copy->addCustomVariable(
            Hauler::noHauler,
            QStringLiteral("vehicle_id"),
            metadata.vehicleId);
        copy->setContainerCurrentLocation(currentLocation);
        copy->addDestination(destination);
        result.append(copy);
    }
    return result;
}

} // namespace

bool SimulationRequestBuilder::buildTrainSegment(
    CargoNetSim::Backend::Path              *path,
    int                                      segmentIdx,
    const QString                           &startId,
    const QString                           &endId,
    int                                      trainContainerCount,
    const QList<ContainerCore::Container *> &containersAvailable,
    int                                     &trainCounter,
    int                                     &containerCounter,
    SimulationRequestBundle                 &bundle,
    QString                                 *err)
{
    qCDebug(lcScenario) << "SimulationRequestBuilder::buildTrainSegment: from:" << startId
                        << "to:" << endId << "segment:" << segmentIdx;
    const auto startLinkages =
        m_doc.linkagesFor(startId, NetworkSpec::Type::Rail);
    const auto endLinkages =
        m_doc.linkagesFor(endId, NetworkSpec::Type::Rail);
    const auto commonNets =
        intersectLinkagesByNetwork(startLinkages, endLinkages);

    const QString regionName =
        m_doc.terminals.value(startId).region;

    for (const auto &pair : commonNets)
    {
        auto *trainNetwork = NetworkLookup::findRail(
            m_registry, regionName, pair.start.networkName);
        if (!trainNetwork)
            continue;
        const auto *startNode = trainNetwork->getNodeByID(pair.start.nodeId);
        const auto *endNode   = trainNetwork->getNodeByID(pair.end.nodeId);
        if (!startNode || !endNode)
            continue;

        const QString networkName = trainNetwork->getNetworkName();
        bundle.trainNetworks[networkName] = trainNetwork;

        QList<ContainerCore::Container *> containersCopy =
            containersAvailable;

        const int numTrains = numVehiclesNeeded(
            containersCopy.size(), trainContainerCount);

        for (int i = 0; i < numTrains; ++i)
        {
            const QString trainId =
                RuntimeArtifacts::vehicleId(
                    path, segmentIdx, trainCounter++,
                    QStringLiteral("train"));

            auto *baseTrain = m_vehicles->getRandomTrain();
            if (!baseTrain)
            {
                if (err) *err = QStringLiteral("No train available for segment dispatch");
                return false;
            }
            auto *train = baseTrain->copy();
            train->setUserId(trainId);

            QVector<int> trainPath;
            trainPath << pair.start.nodeId << pair.end.nodeId;
            train->setTrainPathOnNodeIds(trainPath);
            train->setLoadTime(static_cast<float>(trainCounter * 100));

            TrainSimData td;
            td.train      = train;
            const TerminalHandlingMetadata metadata{
                m_executionId,
                path->canonicalPathKey(),
                endId,
                QString(),
                segmentIdx + 1,
                segmentIdx,
                QStringLiteral("Train"),
                trainId};
            td.containers = takeContainersForVehicle(
                qMin(trainContainerCount, containersCopy.size()),
                containersCopy, path, segmentIdx, containerCounter,
                QString::number(pair.start.nodeId),
                QString::number(pair.end.nodeId), metadata);
            qCInfo(lcScenario)
                << "SimulationRequestBuilder::buildTrainSegment:"
                << "pathId=" << path->getPathId()
                << "pathKey=" << path->canonicalPathKey()
                << "segment=" << segmentIdx
                << "trainId=" << trainId
                << "network=" << networkName
                << "scenarioStart=" << startId
                << "scenarioEnd=" << endId
                << "runtimeStartNode=" << pair.start.nodeId
                << "runtimeEndNode=" << pair.end.nodeId
                << "containerCount=" << td.containers.size()
                << "containerIds=[" << summarizeContainerIds(td.containers)
                << "]";
            bundle.trainData[networkName].append(td);
        }
    }

    if (err)
        err->clear();
    return true;
}

bool SimulationRequestBuilder::buildTruckSegment(
    CargoNetSim::Backend::Path              *path,
    int                                      segmentIdx,
    const QString                           &startId,
    const QString                           &endId,
    int                                      truckContainerCount,
    const QList<ContainerCore::Container *> &containersAvailable,
    int                                     &truckCounter,
    int                                     &containerCounter,
    SimulationRequestBundle                 &bundle,
    QString                                 *err)
{
    qCDebug(lcScenario) << "SimulationRequestBuilder::buildTruckSegment: from:" << startId
                        << "to:" << endId << "segment:" << segmentIdx;
    const auto startLinkages =
        m_doc.linkagesFor(startId, NetworkSpec::Type::Truck);
    const auto endLinkages =
        m_doc.linkagesFor(endId, NetworkSpec::Type::Truck);
    const auto commonNets =
        intersectLinkagesByNetwork(startLinkages, endLinkages);

    const QString regionName =
        m_doc.terminals.value(startId).region;

    // Truck containers carry terminal NAMES (not node IDs) as their
    // location/destination — matches SimulationValidationWorker:702-711.
    // Fallback to the terminal id is consistent with
    // ScenarioApplier::applyTerminals line 207.
    const auto &startPlacement = m_doc.terminals.value(startId);
    const auto &endPlacement   = m_doc.terminals.value(endId);
    const QString startName =
        startPlacement.properties.value("Name", startId).toString();
    const QString endName =
        endPlacement.properties.value("Name", endId).toString();

    for (const auto &pair : commonNets)
    {
        auto *truckNetwork = NetworkLookup::findTruck(
            m_registry, regionName, pair.start.networkName);
        if (!truckNetwork)
            continue;

        const QString networkName = truckNetwork->getNetworkName();
        bundle.truckNetworks[networkName] = truckNetwork;

        QList<ContainerCore::Container *> containersCopy =
            containersAvailable;

        const int numTrucks = numVehiclesNeeded(
            containersCopy.size(), truckContainerCount);

        for (int i = 0; i < numTrucks; ++i)
        {
            TruckSimData td;
            td.tripId = RuntimeArtifacts::vehicleId(
                path, segmentIdx, truckCounter++,
                QStringLiteral("truck"));
            td.originNode      = pair.start.nodeId;
            td.destinationNode = pair.end.nodeId;
            const TerminalHandlingMetadata metadata{
                m_executionId,
                path->canonicalPathKey(),
                endId,
                QString(),
                segmentIdx + 1,
                segmentIdx,
                QStringLiteral("Truck"),
                td.tripId};
            td.containers      = takeContainersForVehicle(
                qMin(truckContainerCount, containersCopy.size()),
                containersCopy, path, segmentIdx, containerCounter,
                startName, endName, metadata);
            bundle.truckData[networkName].append(td);
        }
    }

    if (err)
        err->clear();
    return true;
}

bool SimulationRequestBuilder::buildShipSegment(
    CargoNetSim::Backend::Path              *path,
    int                                      segmentIdx,
    const QString                           &startId,
    const QString                           &endId,
    int                                      shipContainerCount,
    const QList<ContainerCore::Container *> &containersAvailable,
    int                                     &shipCounter,
    int                                     &containerCounter,
    SimulationRequestBundle                 &bundle,
    QString                                 *err)
{
    qCDebug(lcScenario) << "SimulationRequestBuilder::buildShipSegment: from:" << startId
                        << "to:" << endId << "segment:" << segmentIdx;
    const auto &startPlacement = m_doc.terminals.value(startId);
    const auto &endPlacement   = m_doc.terminals.value(endId);

    // Same-region vs cross-region network naming — matches
    // SimulationValidationWorker.cpp:770-781.
    const QString networkName =
        (startPlacement.region == endPlacement.region)
            ? startPlacement.region
            : startPlacement.region + "_to_" + endPlacement.region;

    const QPointF startGlobalPos = m_doc.globalPositionOf(startId);
    const QPointF endGlobalPos   = m_doc.globalPositionOf(endId);

    QList<ContainerCore::Container *> containersCopy =
        containersAvailable;

    const int numShips = numVehiclesNeeded(
        containersCopy.size(), shipContainerCount);

    for (int i = 0; i < numShips; ++i)
    {
        const QString shipId =
            RuntimeArtifacts::vehicleId(
                path, segmentIdx, shipCounter++,
                QStringLiteral("ship"));

        auto *ship = m_vehicles->getRandomShip()->copy();
        ship->setUserId(shipId);

        QVector<QPointF> shipPath;
        shipPath << startGlobalPos << endGlobalPos;
        ship->setPathCoordinates(shipPath);

        ShipSimData sd;
        sd.ship                = ship;
        sd.destinationTerminal = endId;
        const TerminalHandlingMetadata metadata{
            m_executionId,
            path->canonicalPathKey(),
            endId,
            QString(),
            segmentIdx + 1,
            segmentIdx,
            QStringLiteral("Ship"),
            shipId};
        sd.containers          = takeContainersForVehicle(
            qMin(shipContainerCount, containersCopy.size()),
            containersCopy, path, segmentIdx, containerCounter,
            startId, endId, metadata);
        qCInfo(lcScenario)
            << "SimulationRequestBuilder::buildShipSegment:"
            << "pathId=" << path->getPathId()
            << "pathKey=" << path->canonicalPathKey()
            << "segment=" << segmentIdx
            << "shipId=" << shipId
            << "network=" << networkName
            << "scenarioStart=" << startId
            << "scenarioEnd=" << endId
            << "destinationTerminal=" << sd.destinationTerminal
            << "containerCount=" << sd.containers.size()
            << "containerIds=[" << summarizeContainerIds(sd.containers)
            << "]";
        bundle.shipData[networkName].append(sd);
    }

    if (err)
        err->clear();
    return true;
}

bool SimulationRequestBuilder::build(
    const QList<CargoNetSim::Backend::Path *> &paths,
    const PathAllocation                      &allocation,
    SimulationRequestBundle                   &bundle,
    QString                                   *err)
{
    qCInfo(lcScenario) << "SimulationRequestBuilder::build: paths:" << paths.size();
    for (auto *p : paths)
    {
        qCDebug(lcScenario) << "SimulationRequestBuilder::build: processing pathId:"
                            << p->getPathId();
        const auto perPathPool = allocation.containersForPath(p);
        SimulationRequestBundle perPath;
        QString                 localErr;
        if (!buildForPath(p, perPathPool, perPath, &localErr))
        {
            qCWarning(lcScenario) << "SimulationRequestBuilder::build: buildForPath failed for"
                                  << p->getPathId() << ":" << localErr;
            if (err)
                *err = localErr;
            return false;
        }
        bundle.merge(perPath);
    }
    if (err)
        err->clear();
    return true;
}

bool SimulationRequestBuilder::buildForPath(
    CargoNetSim::Backend::Path              *path,
    const QList<ContainerCore::Container *> &originContainers,
    SimulationRequestBundle                 &bundle,
    QString                                 *err)
{
    if (!path)
    {
        qCWarning(lcScenario) << "SimulationRequestBuilder::buildForPath: null path";
        if (err)
            *err = QStringLiteral("path is null");
        return false;
    }
    qCDebug(lcScenario) << "SimulationRequestBuilder::buildForPath: pathId:" << path->getPathId()
                        << "segments:" << path->getSegments().size();

    using Mode = TransportationTypes::TransportationMode;
    const auto transModes = m_config->getTransportModes();
    const int  trainN     = transModes.value(transportationModeToString(Mode::Train)).toMap()
                            .value(PropertyKeys::Mode::AverageContainerNumber, -1).toInt();
    const int  shipN      = transModes.value(transportationModeToString(Mode::Ship)).toMap()
                            .value(PropertyKeys::Mode::AverageContainerNumber, -1).toInt();
    const int  truckN     = transModes.value(transportationModeToString(Mode::Truck)).toMap()
                            .value(PropertyKeys::Mode::AverageContainerNumber, -1).toInt();
    if (trainN < 0 || shipN < 0 || truckN < 0)
    {
        qCWarning(lcScenario) << "SimulationRequestBuilder::buildForPath:"
                              << "container count < 0 (train=" << trainN
                              << "ship=" << shipN << "truck=" << truckN << ")";
        if (err)
            *err = QStringLiteral(
                "Container count cannot be less than 0!");
        return false;
    }

    int shipCounter      = 0;
    int trainCounter     = 0;
    int truckCounter     = 0;
    int containerCounter = 0;

    using Mode = TransportationTypes::TransportationMode;

    const auto segments = path->getSegments();
    for (int i = 0; i < segments.size(); ++i)
    {
        auto *segment = segments[i];
        if (!segment)
        {
            qCDebug(lcScenario) << "SimulationRequestBuilder::buildForPath:"
                                << "skipping null segment at index" << i;
            continue;
        }
        const QString startId = segment->getStart();
        const QString endId   = segment->getEnd();
        if (!m_doc.terminals.contains(startId)
            || !m_doc.terminals.contains(endId))
        {
            qCDebug(lcScenario) << "SimulationRequestBuilder::buildForPath:"
                                << "skipping segment" << i
                                << "with unknown terminal (start=" << startId
                                << "end=" << endId << ")";
            continue;
        }

        const Mode mode = segment->getMode();
        if (mode == Mode::Train)
        {
            if (!buildTrainSegment(path, i, startId, endId, trainN,
                                   originContainers, trainCounter,
                                   containerCounter, bundle, err))
                return false;
        }
        else if (mode == Mode::Truck)
        {
            if (!buildTruckSegment(path, i, startId, endId, truckN,
                                   originContainers, truckCounter,
                                   containerCounter, bundle, err))
                return false;
        }
        else if (mode == Mode::Ship)
        {
            if (!buildShipSegment(path, i, startId, endId, shipN,
                                  originContainers, shipCounter,
                                  containerCounter, bundle, err))
                return false;
        }
    }

    if (err)
        err->clear();
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
