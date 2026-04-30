#include "NetworkExecutionSessionManager.h"

#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/SimulatorCommandAvailability.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

bool isSimulatorCompletionEvent(const ExecutionEventType type)
{
    return type == ExecutionEventType::SegmentVehicleArrived
        || type == ExecutionEventType::SegmentUnloadSucceeded
        || type == ExecutionEventType::SegmentUnloadFailed
        || type == ExecutionEventType::SegmentExecutionFailed;
}

} // namespace

NetworkExecutionSessionManager::NetworkExecutionSessionManager(
    const ScenarioRegistry              &registry,
    RegionDataController                *regionDataController,
    TrainClient::TrainSimulationClient  *trainClient,
    ShipClient::ShipSimulationClient    *shipClient,
    TruckClient::TruckSimulationManager *truckManager,
    const QString                       &truckExecutablePath)
    : m_registry(registry)
    , m_regionDataController(regionDataController)
    , m_trainClient(trainClient)
    , m_shipClient(shipClient)
    , m_truckManager(truckManager)
    , m_truckExecutablePath(truckExecutablePath)
{
}

void NetworkExecutionSessionManager::clear()
{
    m_trainModeInitialized = false;
    m_shipModeInitialized = false;
    m_truckModeInitialized = false;
    m_sessions.clear();
    m_sessionKeyByVehicleId.clear();
}

bool NetworkExecutionSessionManager::dispatchWave(
    const ScenarioExecutionPlan              &plan,
    const SimulationRequestBundle            &bundle,
    const QVector<VehicleDispatchAssignment> &assignments,
    QString                                  *err)
{
    if (!bundle.trainData.isEmpty() && !dispatchTrainWave(bundle, err))
        return false;
    if (!bundle.shipData.isEmpty() && !dispatchShipWave(bundle, err))
        return false;

    QHash<QString, QString> regionByTruckNetwork;
    for (const auto &assignment : assignments)
    {
        const auto *segmentPlan = findSegmentPlan(plan, assignment);
        if (!segmentPlan)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Dispatch wave assignment references an unknown segment for %1[%2]")
                           .arg(assignment.executionPathKey)
                           .arg(assignment.segmentIndex);
            }
            return false;
        }

        if (segmentPlan->mode
            != TransportationTypes::TransportationMode::Truck)
        {
            continue;
        }

        const auto existing =
            regionByTruckNetwork.constFind(assignment.networkName);
        if (existing != regionByTruckNetwork.constEnd()
            && existing.value() != segmentPlan->regionName)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck dispatch wave mixes regions for network %1")
                           .arg(assignment.networkName);
            }
            return false;
        }
        regionByTruckNetwork.insert(assignment.networkName,
                                    segmentPlan->regionName);
    }

    if (!bundle.truckData.isEmpty()
        && !dispatchTruckWave(regionByTruckNetwork, bundle, err))
    {
        return false;
    }

    return registerAssignments(plan, assignments, err);
}

bool NetworkExecutionSessionManager::registerAssignments(
    const ScenarioExecutionPlan              &plan,
    const QVector<VehicleDispatchAssignment> &assignments,
    QString                                  *err)
{
    for (const auto &assignment : assignments)
    {
        const auto *segmentPlan = findSegmentPlan(plan, assignment);
        if (!segmentPlan)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Session registration references an unknown segment for %1[%2]")
                           .arg(assignment.executionPathKey)
                           .arg(assignment.segmentIndex);
            }
            return false;
        }

        const QString key = sessionKey(segmentPlan->mode,
                                       assignment.networkName);
        auto &record = m_sessions[key];
        record.state.networkName = assignment.networkName;
        record.state.mode = segmentPlan->mode;
        record.state.defined = true;
        record.state.running = true;
        record.state.paused = false;
        record.regionName = segmentPlan->regionName;

        const QString activeSegmentKey =
            segmentKey(assignment.executionPathKey,
                       assignment.segmentIndex);
        record.activeSimulatorVehicleIds.insert(assignment.vehicleId);
        record.segmentKeyByVehicleId.insert(assignment.vehicleId,
                                            activeSegmentKey);
        record.activeSimulatorVehicleCountsBySegmentKey[activeSegmentKey] += 1;
        record.state.activeSegmentCount =
            record.activeSimulatorVehicleCountsBySegmentKey.size();

        m_sessionKeyByVehicleId.insert(assignment.vehicleId, key);
    }

    if (err)
        err->clear();
    return true;
}

bool NetworkExecutionSessionManager::advanceActiveSessions(
    double deltaTSeconds, QString *err)
{
    if (deltaTSeconds <= 0.0)
    {
        if (err)
        {
            *err = QStringLiteral(
                "Session advance requires a positive time step");
        }
        return false;
    }

    QStringList activeTrainNetworks;
    QStringList activeShipNetworks;
    QStringList activeTruckNetworks;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd();
         ++it)
    {
        if (it.value().activeSimulatorVehicleIds.isEmpty())
            continue;

        switch (it.value().state.mode)
        {
        case TransportationTypes::TransportationMode::Train:
            activeTrainNetworks.append(it.value().state.networkName);
            break;
        case TransportationTypes::TransportationMode::Ship:
            activeShipNetworks.append(it.value().state.networkName);
            break;
        case TransportationTypes::TransportationMode::Truck:
            activeTruckNetworks.append(it.value().state.networkName);
            break;
        default:
            break;
        }
    }

    if (!activeTrainNetworks.isEmpty())
    {
        if (!m_trainClient
            || !m_trainClient->advanceByTimeStep(activeTrainNetworks,
                                                 deltaTSeconds))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to advance train execution sessions");
            }
            return false;
        }
    }

    if (!activeShipNetworks.isEmpty())
    {
        if (!m_shipClient
            || !m_shipClient->advanceByTimeStep(activeShipNetworks,
                                                deltaTSeconds))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to advance ship execution sessions");
            }
            return false;
        }
    }

    if (!activeTruckNetworks.isEmpty())
    {
        if (!m_truckManager)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck execution sessions require a truck manager");
            }
            return false;
        }

        for (const auto &networkName : activeTruckNetworks)
        {
            auto *client = m_truckManager->getClient(networkName);
            if (!client
                || !client->advanceByTimeStep({networkName},
                                              deltaTSeconds))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to advance truck execution session for %1")
                               .arg(networkName);
                }
                return false;
            }
        }
    }

    if (err)
        err->clear();
    return true;
}

void NetworkExecutionSessionManager::observeExecutionEvent(
    const ExecutionEvent &event)
{
    if (event.vehicleId.isEmpty())
        return;

    const auto sessionKeyIt =
        m_sessionKeyByVehicleId.constFind(event.vehicleId);
    if (sessionKeyIt == m_sessionKeyByVehicleId.constEnd())
        return;

    auto recordIt = m_sessions.find(sessionKeyIt.value());
    if (recordIt == m_sessions.end())
        return;

    auto &record = recordIt.value();
    if (!isSimulatorCompletionEvent(event.type)
        || record.completedSimulatorVehicleIds.contains(event.vehicleId))
    {
        return;
    }

    record.completedSimulatorVehicleIds.insert(event.vehicleId);
    record.activeSimulatorVehicleIds.remove(event.vehicleId);
    const QString activeSegmentKey =
        record.segmentKeyByVehicleId.take(event.vehicleId);
    if (!activeSegmentKey.isEmpty())
    {
        auto countIt =
            record.activeSimulatorVehicleCountsBySegmentKey.find(
                activeSegmentKey);
        if (countIt != record.activeSimulatorVehicleCountsBySegmentKey.end())
        {
            countIt.value() -= 1;
            if (countIt.value() <= 0)
                record.activeSimulatorVehicleCountsBySegmentKey.erase(countIt);
        }
    }

    record.state.activeSegmentCount =
        record.activeSimulatorVehicleCountsBySegmentKey.size();
    if (record.activeSimulatorVehicleIds.isEmpty())
    {
        record.state.running = false;
        record.state.paused = false;
    }
}

bool NetworkExecutionSessionManager::hasActiveVehicles() const
{
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd();
         ++it)
    {
        if (!it.value().activeSimulatorVehicleIds.isEmpty())
            return true;
    }
    return false;
}

QHash<QString, NetworkExecutionSessionState>
NetworkExecutionSessionManager::sessionStates() const
{
    QHash<QString, NetworkExecutionSessionState> states;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd();
         ++it)
    {
        states.insert(it.key(), it.value().state);
    }
    return states;
}

QString NetworkExecutionSessionManager::sessionKey(
    TransportationTypes::TransportationMode mode,
    const QString                          &networkName) const
{
    return QStringLiteral("%1|%2")
        .arg(QString::number(static_cast<int>(mode)),
             networkName);
}

QString NetworkExecutionSessionManager::segmentKey(
    const QString &executionPathKey, int segmentIndex) const
{
    return QStringLiteral("%1|%2")
        .arg(executionPathKey)
        .arg(segmentIndex);
}

const SegmentExecutionPlan *
NetworkExecutionSessionManager::findSegmentPlan(
    const ScenarioExecutionPlan              &plan,
    const VehicleDispatchAssignment          &assignment) const
{
    const auto *pathPlan = plan.findPath(assignment.executionPathKey);
    if (!pathPlan)
        return nullptr;

    if (assignment.segmentIndex < 0
        || assignment.segmentIndex >= pathPlan->segments.size())
    {
        return nullptr;
    }

    return &pathPlan->segments[assignment.segmentIndex];
}

bool NetworkExecutionSessionManager::ensureTrainModeInitialized(
    QString *err)
{
    if (m_trainModeInitialized)
        return true;

    if (!isCommandAvailable(m_trainClient))
    {
        if (err)
        {
            *err = QStringLiteral(
                "Train session dispatch requires an available train client command queue");
        }
        return false;
    }

    m_trainClient->resetServer();
    m_trainModeInitialized = true;
    return true;
}

bool NetworkExecutionSessionManager::ensureShipModeInitialized(
    QString *err)
{
    if (m_shipModeInitialized)
        return true;

    if (!isCommandAvailable(m_shipClient))
    {
        if (err)
        {
            *err = QStringLiteral(
                "Ship session dispatch requires an available ship client command queue");
        }
        return false;
    }

    // ShipNetSim already supports per-network replacement in
    // defineSimulator/loadNetwork. The live execution engine owns
    // sessions by network, so forcing a global reset here is the
    // wrong boundary: it can block while tearing down unrelated
    // in-flight simulator state and it undermines persistent
    // session ownership.
    m_shipModeInitialized = true;
    return true;
}

bool NetworkExecutionSessionManager::ensureTruckModeInitialized(
    QString *err)
{
    if (m_truckModeInitialized)
        return true;

    if (!m_truckManager)
    {
        if (err)
        {
            *err = QStringLiteral(
                "Truck session dispatch requires a truck manager");
        }
        return false;
    }

    if (m_truckExecutablePath.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                "Truck session dispatch requires a non-empty truck executable path");
        }
        return false;
    }

    m_truckManager->resetServer();
    m_truckModeInitialized = true;
    return true;
}

bool NetworkExecutionSessionManager::dispatchTrainWave(
    const SimulationRequestBundle &bundle, QString *err)
{
    if (bundle.trainData.isEmpty())
        return true;
    if (!ensureTrainModeInitialized(err))
        return false;

    for (auto it = bundle.trainData.constBegin();
         it != bundle.trainData.constEnd(); ++it)
    {
        const QString networkName = it.key();
        const auto sessionKeyValue = sessionKey(
            TransportationTypes::TransportationMode::Train,
            networkName);
        auto &record = m_sessions[sessionKeyValue];
        record.state.networkName = networkName;
        record.state.mode =
            TransportationTypes::TransportationMode::Train;

        QList<CargoNetSim::Backend::Train *> trains;
        for (const auto &dispatch : it.value())
            trains.append(dispatch.train);

        if (!record.state.defined)
        {
            auto *network = bundle.trainNetworks.value(
                networkName, nullptr);
            if (!network)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Train dispatch requires a resolved network for %1")
                               .arg(networkName);
                }
                return false;
            }
            if (!m_trainClient->defineSimulator(network, 1.0,
                                                trains))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to define train execution session for %1")
                               .arg(networkName);
                }
                return false;
            }
            record.state.defined = true;
        }
        else if (!trains.isEmpty()
                 && !m_trainClient->addTrainsToSimulator(
                     networkName, trains))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to add trains to existing train session %1")
                           .arg(networkName);
            }
            return false;
        }

        for (const auto &dispatch : it.value())
        {
            if (!dispatch.containers.isEmpty()
                && !m_trainClient->addContainersToTrain(
                    networkName, dispatch.train->getUserId(),
                    dispatch.containers))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to add containers to train %1 on %2")
                               .arg(dispatch.train->getUserId(),
                                    networkName);
                }
                return false;
            }
        }
    }

    return true;
}

bool NetworkExecutionSessionManager::dispatchShipWave(
    const SimulationRequestBundle &bundle, QString *err)
{
    if (bundle.shipData.isEmpty())
        return true;
    if (!ensureShipModeInitialized(err))
        return false;

    for (auto it = bundle.shipData.constBegin();
         it != bundle.shipData.constEnd(); ++it)
    {
        const QString networkName = it.key();
        const auto sessionKeyValue = sessionKey(
            TransportationTypes::TransportationMode::Ship,
            networkName);
        auto &record = m_sessions[sessionKeyValue];
        record.state.networkName = networkName;
        record.state.mode =
            TransportationTypes::TransportationMode::Ship;

        QList<CargoNetSim::Backend::Ship *> ships;
        QMap<QString, QStringList> destinationTerminals;
        for (const auto &dispatch : it.value())
        {
            ships.append(dispatch.ship);
            destinationTerminals.insert(dispatch.ship->getUserId(),
                                        {dispatch.destinationTerminal});
        }

        if (!record.state.defined)
        {
            if (!m_shipClient->defineSimulator(
                    networkName, 1.0, ships, destinationTerminals,
                    QString()))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to define ship execution session for %1")
                               .arg(networkName);
                }
                return false;
            }
            record.state.defined = true;
        }
        else if (!ships.isEmpty()
                 && !m_shipClient->addShipsToSimulator(
                     networkName, ships, destinationTerminals))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to add ships to existing ship session %1")
                           .arg(networkName);
            }
            return false;
        }

        for (const auto &dispatch : it.value())
        {
            if (!dispatch.containers.isEmpty()
                && !m_shipClient->addContainersToShip(
                    networkName, dispatch.ship->getUserId(),
                    dispatch.containers))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to add containers to ship %1 on %2")
                               .arg(dispatch.ship->getUserId(),
                                    networkName);
                }
                return false;
            }
        }
    }

    return true;
}

bool NetworkExecutionSessionManager::dispatchTruckWave(
    const QHash<QString, QString> &regionByTruckNetwork,
    const SimulationRequestBundle &bundle,
    QString                      *err)
{
    if (bundle.truckData.isEmpty())
        return true;
    if (!ensureTruckModeInitialized(err))
        return false;

    for (auto it = bundle.truckData.constBegin();
         it != bundle.truckData.constEnd(); ++it)
    {
        const QString networkName = it.key();
        const QString regionName =
            regionByTruckNetwork.value(networkName);
        if (regionName.isEmpty())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck dispatch requires a resolved region for %1")
                           .arg(networkName);
            }
            return false;
        }

        auto *config = NetworkLookup::findTruckConfig(
            m_registry, regionName, networkName);
        if (!config)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck dispatch requires a runtime config for %1")
                           .arg(networkName);
            }
            return false;
        }

        const QString masterConfigPath =
            config->getSourceConfigPath();
        if (masterConfigPath.isEmpty())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck dispatch requires the source master config path for %1")
                           .arg(networkName);
            }
            return false;
        }

        const auto sessionKeyValue = sessionKey(
            TransportationTypes::TransportationMode::Truck,
            networkName);
        auto &record = m_sessions[sessionKeyValue];
        record.state.networkName = networkName;
        record.state.mode =
            TransportationTypes::TransportationMode::Truck;
        record.regionName = regionName;

        if (!record.state.defined)
        {
            TruckClient::ClientConfiguration clientConfig;
            clientConfig.exePath = m_truckExecutablePath;
            clientConfig.masterFilePath = masterConfigPath;
            clientConfig.simTime = config->getSimTime();
            if (!m_truckManager->createClient(networkName,
                                              clientConfig))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to define truck execution session for %1")
                               .arg(networkName);
                }
                return false;
            }
            record.state.defined = true;
        }

        auto *client = m_truckManager->getClient(networkName);
        if (!client)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Truck execution session for %1 has no client")
                           .arg(networkName);
            }
            return false;
        }

        for (const auto &dispatch : it.value())
        {
            const QString tripId = client->addTrip(
                networkName,
                QString::number(dispatch.originNode),
                QString::number(dispatch.destinationNode),
                dispatch.containers);
            if (tripId.isEmpty())
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Failed to add trip to truck session %1")
                               .arg(networkName);
                }
                return false;
            }
        }
    }

    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
