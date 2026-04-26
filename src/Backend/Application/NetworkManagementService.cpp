#include "Backend/Application/NetworkManagementService.h"

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Models/BaseNetwork.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

RegionDataController *regionDataController(
    ::CargoNetSim::CargoNetSimController *controller)
{
    return controller ? controller->getRegionDataController()
                      : nullptr;
}

RegionData *regionDataFor(
    ::CargoNetSim::CargoNetSimController *controller,
    const QString                        &regionName)
{
    auto *rdc = regionDataController(controller);
    return (rdc && !regionName.isEmpty())
               ? rdc->getRegionData(regionName)
               : nullptr;
}

BaseNetwork *baseNetworkFor(RegionData         *regionData,
                            const QString      &networkName,
                            CargoNetSim::Backend::NetworkKind kind)
{
    if (!regionData || networkName.isEmpty())
        return nullptr;

    switch (kind)
    {
    case NetworkKind::Rail:
        return regionData->getTrainNetwork(networkName);
    case NetworkKind::Truck:
        return regionData->getTruckNetwork(networkName);
    }

    return nullptr;
}

} // namespace

NetworkManagementService::NetworkManagementService(
    ::CargoNetSim::CargoNetSimController *controller)
    : m_controller(controller)
{
}

::CargoNetSim::CargoNetSimController *
NetworkManagementService::controller() const
{
    if (m_controller)
        return m_controller;
    return ::CargoNetSim::CargoNetSimController::instance();
}

bool NetworkManagementService::checkNetworkNameConflict(
    const QString &regionName,
    const QString &networkName) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return false;

    return regionData->checkNetworkNameConflict(networkName);
}

bool NetworkManagementService::addTrainNetwork(
    const QString &regionName,
    const QString &networkName,
    const QString &nodeFile,
    const QString &linkFile) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return false;

    regionData->addTrainNetwork(networkName, nodeFile, linkFile);
    return true;
}

bool NetworkManagementService::addTruckNetwork(
    const QString &regionName,
    const QString &networkName,
    const QString &configFile) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return false;

    regionData->addTruckNetwork(networkName, configFile);
    return true;
}

bool NetworkManagementService::removeNetwork(
    const QString &regionName,
    const QString &networkName,
    NetworkKind    kind) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return false;

    switch (kind)
    {
    case NetworkKind::Rail:
        return regionData->removeTrainNetwork(networkName);
    case NetworkKind::Truck:
        return regionData->removeTruckNetwork(networkName);
    }

    return false;
}

bool NetworkManagementService::renameNetwork(
    const QString &regionName,
    const QString &oldName,
    const QString &newName,
    NetworkKind    kind) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || oldName.isEmpty() || newName.isEmpty())
        return false;

    switch (kind)
    {
    case NetworkKind::Rail:
        return regionData->renameTrainNetwork(oldName, newName);
    case NetworkKind::Truck:
        return regionData->renameTruckNetwork(oldName, newName);
    }

    return false;
}

bool NetworkManagementService::networkExists(
    const QString &regionName,
    const QString &networkName,
    NetworkKind    kind) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return false;

    switch (kind)
    {
    case NetworkKind::Rail:
        return regionData->trainNetworkExists(networkName);
    case NetworkKind::Truck:
        return regionData->truckNetworkExists(networkName);
    }

    return false;
}

bool NetworkManagementService::setNetworkVariable(
    const QString  &regionName,
    const QString  &networkName,
    NetworkKind     kind,
    const QString  &key,
    const QVariant &value) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    auto *network =
        baseNetworkFor(regionData, networkName, kind);
    if (!network || key.isEmpty())
        return false;

    network->setVariable(key, value);
    return true;
}

ShortestPathResult NetworkManagementService::findShortestPath(
    const QString &regionName,
    const QString &networkName,
    NetworkKind    kind,
    int            startNodeId,
    int            endNodeId) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || networkName.isEmpty())
        return ShortestPathResult();

    switch (kind)
    {
    case NetworkKind::Rail: {
        auto *network =
            regionData->getTrainNetwork(networkName);
        return network ? network->findShortestPath(startNodeId, endNodeId)
                       : ShortestPathResult();
    }
    case NetworkKind::Truck: {
        auto *network =
            regionData->getTruckNetwork(networkName);
        return network ? network->findShortestPath(startNodeId, endNodeId)
                       : ShortestPathResult();
    }
    }

    return ShortestPathResult();
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
