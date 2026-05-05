#include "Backend/Application/NetworkManagementService.h"

#include "Backend/Application/ScenarioEditService.h"
#include "Backend/Clients/TruckClient/IntegrationNode.h"
#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/GeoProjection.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Models/BaseNetwork.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"

#include <cmath>
#include <limits>

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

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool sameNetworkNode(const Scenario::NodeLinkage &linkage,
                     const QString               &networkName,
                     int                          nodeId)
{
    return linkage.networkName == networkName
        && linkage.nodeId == nodeId;
}

bool networkNodeOccupied(const Scenario::ScenarioDocument &doc,
                         const QString                    &regionName,
                         const QString                    &networkName,
                         int                               nodeId)
{
    for (const Scenario::NodeLinkage &linkage : doc.linkages)
    {
        const auto terminalIt =
            doc.terminals.constFind(linkage.terminalId);
        if (!linkage.excluded
            && terminalIt != doc.terminals.constEnd()
            && terminalIt->region == regionName
            && sameNetworkNode(linkage, networkName, nodeId))
        {
            return true;
        }
    }
    return false;
}

bool terminalLinkedToRequestedKind(
    const Scenario::ScenarioDocument &doc,
    const Scenario::TerminalPlacement &terminal,
    const QList<NetworkKind>          &kinds)
{
    const auto regionIt =
        doc.regions.constFind(terminal.region);
    if (regionIt == doc.regions.constEnd())
        return false;

    for (const Scenario::NodeLinkage &linkage : doc.linkages)
    {
        if (linkage.excluded
            || linkage.terminalId != terminal.id)
        {
            continue;
        }

        const auto networkIt =
            regionIt->networks.constFind(linkage.networkName);
        if (networkIt != regionIt->networks.constEnd()
            && kinds.contains(networkIt->type))
        {
            return true;
        }
    }
    return false;
}

QPointF projectedNodePoint(RegionData    *regionData,
                           const QString &networkName,
                           NetworkKind    kind,
                           int            nodeId)
{
    if (!regionData || networkName.isEmpty() || nodeId < 0)
        return QPointF(std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN());

    switch (kind)
    {
    case NetworkKind::Rail: {
        auto *network = regionData->getTrainNetwork(networkName);
        const auto *node = network ? network->getNodeByID(nodeId)
                                   : nullptr;
        return node ? projectTrainNode(*node)
                    : QPointF(std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN());
    }
    case NetworkKind::Truck: {
        auto *network = regionData->getTruckNetwork(networkName);
        if (!network)
            return QPointF(
                std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN());

        for (auto *node : network->getNodes())
        {
            if (node && node->getNodeId() == nodeId)
                return projectTruckNode(*node);
        }
        return QPointF(std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN());
    }
    }

    return QPointF(std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN());
}

QPointF terminalProjectedPoint(
    const Scenario::ScenarioDocument  &doc,
    const Scenario::TerminalPlacement &terminal,
    RegionData                        *regionData)
{
    switch (terminal.mode)
    {
    case Scenario::TerminalPlacement::PositionMode::LatLon:
        return Commons::GeoProjection::wgs84ToWebMercatorMeters(
            QPointF(terminal.latLon.longitude,
                    terminal.latLon.latitude));
    case Scenario::TerminalPlacement::PositionMode::Scene:
        return QPointF(terminal.scenePos.x, terminal.scenePos.y);
    case Scenario::TerminalPlacement::PositionMode::NetworkNode: {
        const auto regionIt =
            doc.regions.constFind(terminal.region);
        if (regionIt == doc.regions.constEnd())
            break;
        const auto networkIt =
            regionIt->networks.constFind(terminal.networkRef);
        if (networkIt == regionIt->networks.constEnd())
            break;
        return projectedNodePoint(regionData,
                                  terminal.networkRef,
                                  networkIt->type,
                                  terminal.nodeId);
    }
    }

    return QPointF(std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN());
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

bool NetworkManagementService::importNetwork(
    Scenario::ScenarioDocument   *doc,
    const QString                &regionName,
    const QString                &networkName,
    NetworkKind                   kind,
    const QMap<QString, QString> &files,
    QString                      *error) const
{
    if (error)
        error->clear();

    const auto spec =
        ScenarioEditService::buildNetworkSpec(
            doc, regionName, networkName, kind, files, error);
    if (!spec)
        return false;

    auto *regionData = regionDataFor(controller(), regionName);
    if (!regionData)
    {
        if (error)
            *error = QStringLiteral(
                "Region '%1' is not loaded in the runtime.")
                         .arg(regionName);
        return false;
    }

    if (regionData->checkNetworkNameConflict(spec->name))
    {
        if (error)
            *error = QStringLiteral(
                "Network '%1' already exists in region '%2'.")
                         .arg(spec->name, regionName);
        return false;
    }

    try
    {
        switch (kind)
        {
        case NetworkKind::Rail:
            regionData->addTrainNetwork(
                spec->name,
                spec->files.value(QStringLiteral("nodes")),
                spec->files.value(QStringLiteral("links")));
            break;
        case NetworkKind::Truck:
            regionData->addTruckNetwork(
                spec->name,
                spec->files.value(QStringLiteral("config")));
            break;
        }
    }
    catch (const std::exception &e)
    {
        if (error)
            *error = QStringLiteral("Failed to load network '%1': %2")
                         .arg(spec->name,
                              QString::fromUtf8(e.what()));
        return false;
    }

    if (!doc->addNetwork(regionName, spec.value()))
    {
        qCWarning(lcGuiScene)
            << "NetworkManagementService::importNetwork:"
            << "document commit failed; rolling back runtime network"
            << spec->name << "region=" << regionName;
        try
        {
            switch (kind)
            {
            case NetworkKind::Rail:
                regionData->removeTrainNetwork(spec->name);
                break;
            case NetworkKind::Truck:
                regionData->removeTruckNetwork(spec->name);
                break;
            }
        }
        catch (const std::exception &e)
        {
            qCWarning(lcGuiScene)
                << "NetworkManagementService::importNetwork:"
                << "runtime rollback failed for" << spec->name
                << ":" << e.what();
        }
        if (error)
            *error = QStringLiteral(
                "Failed to record network '%1' in the scenario document.")
                         .arg(spec->name);
        return false;
    }

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

LinkTerminalToNodeResult
NetworkManagementService::linkTerminalToClosestNetworkNode(
    Scenario::ScenarioDocument &doc,
    const QString              &terminalId,
    const QList<NetworkKind>   &kinds,
    Scenario::LinkageSource     source) const
{
    LinkTerminalToNodeResult result;

    if (terminalId.isEmpty() || kinds.isEmpty())
    {
        result.status = LinkTerminalToNodeStatus::InvalidRequest;
        result.message =
            QStringLiteral("A terminal and at least one network type are required.");
        return result;
    }

    const auto terminalIt = doc.terminals.constFind(terminalId);
    if (terminalIt == doc.terminals.constEnd())
    {
        result.status = LinkTerminalToNodeStatus::InvalidRequest;
        result.message =
            QStringLiteral("Terminal '%1' does not exist.")
                .arg(terminalId);
        return result;
    }

    const Scenario::TerminalPlacement &terminal = *terminalIt;
    const auto regionIt =
        doc.regions.constFind(terminal.region);
    if (regionIt == doc.regions.constEnd())
    {
        result.status = LinkTerminalToNodeStatus::InvalidRequest;
        result.message =
            QStringLiteral("Terminal '%1' belongs to missing region '%2'.")
                .arg(terminalId, terminal.region);
        return result;
    }

    if (terminalLinkedToRequestedKind(doc, terminal, kinds))
    {
        result.status = LinkTerminalToNodeStatus::AlreadyLinked;
        result.message =
            QStringLiteral("Terminal '%1' is already linked to one of the selected network types.")
                .arg(terminalId);
        return result;
    }

    auto *regionData = regionDataFor(controller(), terminal.region);
    if (!regionData)
    {
        result.status = LinkTerminalToNodeStatus::InvalidRequest;
        result.message =
            QStringLiteral("Region '%1' is not loaded in the runtime.")
                .arg(terminal.region);
        return result;
    }

    const QPointF terminalPoint =
        terminalProjectedPoint(doc, terminal, regionData);
    if (!isFinitePoint(terminalPoint))
    {
        result.status = LinkTerminalToNodeStatus::InvalidRequest;
        result.message =
            QStringLiteral("Terminal '%1' does not have a usable local position.")
                .arg(terminalId);
        return result;
    }

    struct Candidate
    {
        QString networkName;
        int nodeId = -1;
        NetworkKind kind = NetworkKind::Rail;
        int kindOrder = 0;
        double distanceMeters =
            std::numeric_limits<double>::infinity();
    };

    Candidate best;
    bool found = false;

    auto considerCandidate =
        [&](const QString &networkName, int nodeId,
            NetworkKind kind, int kindOrder,
            const QPointF &nodePoint)
    {
        if (nodeId < 0
            || !isFinitePoint(nodePoint)
            || networkNodeOccupied(doc, terminal.region,
                                   networkName, nodeId))
        {
            return;
        }

        const double dx = terminalPoint.x() - nodePoint.x();
        const double dy = terminalPoint.y() - nodePoint.y();
        const double distance = std::hypot(dx, dy);

        const bool better =
            !found
            || distance < best.distanceMeters
            || (distance == best.distanceMeters
                && kindOrder < best.kindOrder)
            || (distance == best.distanceMeters
                && kindOrder == best.kindOrder
                && networkName < best.networkName)
            || (distance == best.distanceMeters
                && kindOrder == best.kindOrder
                && networkName == best.networkName
                && nodeId < best.nodeId);

        if (!better)
            return;

        best.networkName = networkName;
        best.nodeId = nodeId;
        best.kind = kind;
        best.kindOrder = kindOrder;
        best.distanceMeters = distance;
        found = true;
    };

    for (auto it = regionIt->networks.constBegin();
         it != regionIt->networks.constEnd(); ++it)
    {
        const Scenario::NetworkSpec &spec = it.value();
        const int kindOrder = kinds.indexOf(spec.type);
        if (kindOrder < 0)
            continue;

        switch (spec.type)
        {
        case NetworkKind::Rail: {
            auto *network = regionData->getTrainNetwork(spec.name);
            if (!network)
                continue;
            for (auto *node : network->getNodes())
            {
                if (!node)
                    continue;
                considerCandidate(spec.name,
                                  node->getUserId(),
                                  spec.type,
                                  kindOrder,
                                  projectTrainNode(*node));
            }
            break;
        }
        case NetworkKind::Truck: {
            auto *network = regionData->getTruckNetwork(spec.name);
            if (!network)
                continue;
            for (auto *node : network->getNodes())
            {
                if (!node)
                    continue;
                considerCandidate(spec.name,
                                  node->getNodeId(),
                                  spec.type,
                                  kindOrder,
                                  projectTruckNode(*node));
            }
            break;
        }
        }
    }

    if (!found)
    {
        result.status = LinkTerminalToNodeStatus::NoAvailableNode;
        result.message =
            QStringLiteral("No available network node found in region '%1'.")
                .arg(terminal.region);
        return result;
    }

    if (!ScenarioEditService::linkTerminalToNode(
            &doc, terminalId, best.networkName, best.nodeId, source))
    {
        result.status = LinkTerminalToNodeStatus::CommitFailed;
        result.message =
            QStringLiteral("Failed to link terminal '%1' to network '%2' node %3.")
                .arg(terminalId, best.networkName)
                .arg(best.nodeId);
        return result;
    }

    result.status = LinkTerminalToNodeStatus::Success;
    result.message =
        QStringLiteral("Terminal '%1' linked to network '%2' node %3.")
            .arg(terminalId, best.networkName)
            .arg(best.nodeId);
    result.networkName = best.networkName;
    result.nodeId = best.nodeId;
    result.kind = best.kind;
    result.distanceMeters = best.distanceMeters;
    return result;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
