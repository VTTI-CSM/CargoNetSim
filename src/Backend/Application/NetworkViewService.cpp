#include "Backend/Application/NetworkViewService.h"

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/IntegrationNode.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Models/BaseNetwork.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/ScenarioDocument.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

QPointF projectTrainNode(
    const TrainClient::NeTrainSimNode &node)
{
    return QPointF(node.getX() * node.getXScale(),
                   node.getY() * node.getYScale());
}

QPointF projectTruckNode(
    const TruckClient::IntegrationNode &node)
{
    return QPointF(
        Units::toMeters(
            Units::kilometers(node.getXCoordinate()
                              * node.getXScale()))
            .value(),
        Units::toMeters(
            Units::kilometers(node.getYCoordinate()
                              * node.getYScale()))
            .value());
}

TruckClient::IntegrationNode *findTruckNode(
    TruckClient::IntegrationNetwork *network,
    int                              nodeId)
{
    if (!network)
        return nullptr;

    for (auto *node : network->getNodes())
    {
        if (node && node->getNodeId() == nodeId)
            return node;
    }
    return nullptr;
}

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
    return rdc ? rdc->getRegionData(regionName) : nullptr;
}

} // namespace

NetworkViewService::NetworkViewService(
    ::CargoNetSim::CargoNetSimController *controller,
    QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    if (parent)
    {
        if (auto *rdc = regionDataController(this->controller()))
        {
            connect(rdc, &RegionDataController::regionAdded,
                    this, &NetworkViewService::regionAdded);
            connect(rdc, &RegionDataController::regionRenamed,
                    this, &NetworkViewService::regionRenamed);
            connect(rdc, &RegionDataController::regionRemoved,
                    this, &NetworkViewService::regionRemoved);
            connect(rdc, &RegionDataController::currentRegionChanged,
                    this, &NetworkViewService::currentRegionChanged);
            connect(rdc, &RegionDataController::regionsCleared,
                    this, &NetworkViewService::regionsCleared);
            connect(rdc, &RegionDataController::regionVariableChanged,
                    this, &NetworkViewService::regionVariableChanged);
        }
    }
}

::CargoNetSim::CargoNetSimController *
NetworkViewService::controller() const
{
    if (m_controller)
        return m_controller;
    return ::CargoNetSim::CargoNetSimController::instance();
}

QString NetworkViewService::currentRegionName() const
{
    auto *rdc = regionDataController(controller());
    return rdc ? rdc->getCurrentRegion() : QString();
}

bool NetworkViewService::setCurrentRegion(
    const QString &regionName) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || regionName.isEmpty())
        return false;
    rdc->setCurrentRegion(regionName);
    return true;
}

QStringList NetworkViewService::regionNames() const
{
    auto *rdc = regionDataController(controller());
    return rdc ? rdc->getAllRegionNames() : QStringList();
}

bool NetworkViewService::regionExists(
    const QString &regionName) const
{
    return !regionName.isEmpty()
           && regionNames().contains(regionName);
}

QVariant NetworkViewService::regionVariable(
    const QString &regionName,
    const QString &key) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || key.isEmpty())
        return QVariant();
    return regionData->getVariable(key);
}

bool NetworkViewService::addRegion(
    const QString &regionName) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || regionName.isEmpty())
        return false;
    return rdc->addRegion(regionName);
}

bool NetworkViewService::renameRegion(
    const QString &oldName,
    const QString &newName) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || oldName.isEmpty() || newName.isEmpty())
        return false;
    return rdc->renameRegion(oldName, newName);
}

bool NetworkViewService::removeRegion(
    const QString &regionName) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || regionName.isEmpty())
        return false;
    return rdc->removeRegion(regionName);
}

QStringList NetworkViewService::trainNetworkNames(
    const QString &regionName) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    return regionData ? regionData->getTrainNetworks()
                      : QStringList();
}

QStringList NetworkViewService::truckNetworkNames(
    const QString &regionName) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    return regionData ? regionData->getTruckNetworks()
                      : QStringList();
}

QStringList NetworkViewService::networkNames(
    const QString &regionName,
    NetworkKind    kind) const
{
    switch (kind)
    {
    case NetworkKind::Rail:
        return trainNetworkNames(regionName);
    case NetworkKind::Truck:
        return truckNetworkNames(regionName);
    }

    return QStringList();
}

bool NetworkViewService::regionOwnsNetwork(
    const QString &regionName,
    const QString &networkName) const
{
    if (regionName.isEmpty() || networkName.isEmpty())
        return false;

    auto *regionData =
        regionDataFor(controller(), regionName);
    return regionData
           && (regionData->getTrainNetworks().contains(
                   networkName)
               || regionData->getTruckNetworks().contains(
                   networkName));
}

QVariant NetworkViewService::networkVariable(
    const QString &regionName,
    const QString &networkName,
    const QString &key) const
{
    if (networkName.isEmpty() || key.isEmpty())
        return QVariant();

    const auto view =
        resolveNetworkView(regionName, networkName);
    if (!view || !view->networkObject)
        return QVariant();

    auto *baseNetwork = qobject_cast<BaseNetwork *>(
        view->networkObject);
    if (!baseNetwork)
        return QVariant();

    return baseNetwork->getVariable(key);
}

QString NetworkViewService::owningRegionName(
    const Scenario::ScenarioDocument &document,
    const QString                    &networkName) const
{
    if (networkName.isEmpty())
        return QString();

    for (auto it = document.regions.constBegin();
         it != document.regions.constEnd(); ++it)
    {
        if (regionOwnsNetwork(it.key(), networkName))
            return it.key();
    }
    return QString();
}

QString NetworkViewService::networkNameOf(
    QObject     *networkObject,
    NetworkKind *outKind) const
{
    return Scenario::NetworkLookup::networkNameOf(
        networkObject, outKind);
}

std::optional<NetworkResolvedView>
NetworkViewService::resolveNetworkView(
    const QString &regionName,
    const QString &networkName) const
{
    if (regionName.isEmpty() || networkName.isEmpty())
        return std::nullopt;

    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData)
        return std::nullopt;

    NetworkKind kind = NetworkKind::Rail;
    QObject    *networkObject =
        regionData->findNetworkByName(networkName, &kind);
    if (!networkObject)
        return std::nullopt;

    NetworkResolvedView view;
    view.regionName = regionName;
    view.networkName = networkName;
    view.kind = kind;
    view.networkObject = networkObject;
    return view;
}

std::optional<NetworkNodeView>
NetworkViewService::resolveNodeView(
    const QString &regionName,
    const QString &networkName,
    int            nodeId) const
{
    if (nodeId < 0)
        return std::nullopt;

    const auto networkView =
        resolveNetworkView(regionName, networkName);
    if (!networkView)
        return std::nullopt;

    NetworkNodeView view;
    view.regionName = networkView->regionName;
    view.networkName = networkView->networkName;
    view.nodeId = nodeId;
    view.kind = networkView->kind;
    view.networkObject = networkView->networkObject;

    switch (networkView->kind)
    {
    case NetworkKind::Rail: {
        auto *railNetwork = qobject_cast<
            TrainClient::NeTrainSimNetwork *>(networkView->networkObject);
        if (!railNetwork)
            return std::nullopt;
        auto *node = railNetwork->getNodeByID(nodeId);
        if (!node)
            return std::nullopt;
        view.projectedScenePoint = projectTrainNode(*node);
        return view;
    }
    case NetworkKind::Truck: {
        auto *truckNetwork = qobject_cast<
            TruckClient::IntegrationNetwork *>(networkView->networkObject);
        auto *node = findTruckNode(truckNetwork, nodeId);
        if (!node)
            return std::nullopt;
        view.projectedScenePoint = projectTruckNode(*node);
        return view;
    }
    }

    return std::nullopt;
}

bool NetworkViewService::setRegionVariable(
    const QString &regionName,
    const QString &key,
    const QVariant &value) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || regionName.isEmpty() || key.isEmpty())
        return false;
    rdc->setRegionVariable(regionName, key, value);
    return true;
}

bool NetworkViewService::removeRegionVariable(
    const QString &regionName,
    const QString &key) const
{
    auto *regionData =
        regionDataFor(controller(), regionName);
    if (!regionData || key.isEmpty())
        return false;
    regionData->removeVariable(key);
    return true;
}

bool NetworkViewService::setGlobalVariable(
    const QString &key,
    const QVariant &value) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || key.isEmpty())
        return false;
    rdc->setGlobalVariable(key, value);
    return true;
}

bool NetworkViewService::removeGlobalVariable(
    const QString &key) const
{
    auto *rdc = regionDataController(controller());
    if (!rdc || key.isEmpty())
        return false;
    rdc->removeGlobalVariable(key);
    return true;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
