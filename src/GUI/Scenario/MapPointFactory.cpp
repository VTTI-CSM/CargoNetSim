#include "MapPointFactory.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/IntegrationNode.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/NetworkKind.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QMap>
#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

/// Train-node → scene point. Mirrors ViewController.cpp:1349-1351.
QPointF projectTrainNode(
    const CargoNetSim::Backend::TrainClient::NeTrainSimNode &node)
{
    return QPointF(node.getX() * node.getXScale(),
                   node.getY() * node.getYScale());
}

/// Truck-node → scene point. Mirrors ViewController.cpp:1481-1486.
/// IntegrationNode coordinates are in km; scale × 1000 → metres.
QPointF projectTruckNode(
    const CargoNetSim::Backend::TruckClient::IntegrationNode &node)
{
    return QPointF(
        CargoNetSim::Backend::Units::toMeters(
            CargoNetSim::Backend::Units::kilometers(
                node.getXCoordinate() * node.getXScale()))
            .value(),
        CargoNetSim::Backend::Units::toMeters(
            CargoNetSim::Backend::Units::kilometers(
                node.getYCoordinate() * node.getYScale()))
            .value());
}

/// Look up a node by ID in a truck network. IntegrationNetwork has no
/// O(1) by-id accessor; linear scan is the same pattern used by
/// ViewController. Kept file-local so a future by-id method on
/// IntegrationNetwork collapses the call sites.
CargoNetSim::Backend::TruckClient::IntegrationNode *
findTruckNode(
    CargoNetSim::Backend::TruckClient::IntegrationNetwork *net, int id)
{
    if (!net) return nullptr;
    for (auto *n : net->getNodes())
        if (n && n->getNodeId() == id) return n;
    return nullptr;
}

} // namespace

MapPoint *MapPointFactory::findByNetworkAndNode(
    GraphicsScene *scene, const QString &networkName, int nodeId)
{
    qCDebug(lcGuiScene)
        << "MapPointFactory::findByNetworkAndNode:"
        << "network=" << networkName
        << "nodeId=" << nodeId;

    if (!scene) return nullptr;
    const QString targetNodeId = QString::number(nodeId);
    const auto    all = scene->getItemsByType<MapPoint>();
    for (auto *mp : all)
    {
        if (!mp) continue;
        if (mp->getReferencedNetworkNodeID() != targetNodeId) continue;

        // Preferred path: linked MapPoints carry the canonical
        // networkName on their NodeLinkage model.
        if (auto *link = mp->linkageModel())
        {
            if (link->networkName == networkName) return mp;
            continue;
        }
        // Fallback: unlinked MapPoint from a legacy network-import
        // path. Read the name via the shared NetworkLookup helper so
        // the "cast-to-known-subclass + getNetworkName" pattern lives
        // in exactly one place (NetworkLookup::networkNameOf).
        if (Backend::Scenario::NetworkLookup::networkNameOf(
                mp->getReferenceNetwork())
            == networkName)
            return mp;
    }
    return nullptr;
}

MapPoint *MapPointFactory::fromNodeLinkage(
    Backend::Scenario::NodeLinkage *linkage,
    Backend::RegionData            *regionData,
    GraphicsScene                  *scene,
    MainWindow                     *mainWindow)
{
    if (!linkage || !regionData || !scene) return nullptr;

    if (auto *existing = findByNetworkAndNode(
            scene, linkage->networkName, linkage->nodeId))
    {
        qCDebug(lcGuiScene)
            << "MapPointFactory::fromNodeLinkage: reusing existing MapPoint"
            << "network=" << linkage->networkName
            << "nodeId="  << linkage->nodeId;
        existing->setLinkageModel(linkage);
        return existing;
    }

    qCInfo(lcGuiScene)
        << "MapPointFactory::fromNodeLinkage:"
        << "network=" << linkage->networkName
        << "nodeId=" << linkage->nodeId
        << "region=" << regionData->getRegion();

    // Single dispatch: RegionData resolves "rail or truck" for us.
    Backend::NetworkKind kind = Backend::NetworkKind::Rail;
    QObject *network =
        regionData->findNetworkByName(linkage->networkName, &kind);
    if (!network)
    {
        qCWarning(lcGuiScene)
            << "MapPointFactory::fromNodeLinkage:"
            << "network not found:" << linkage->networkName;
        return nullptr;
    }

    QPointF projected;
    switch (kind)
    {
    case Backend::NetworkKind::Rail: {
        auto *railNet =
            qobject_cast<Backend::TrainClient::NeTrainSimNetwork *>(network);
        if (!railNet) return nullptr;
        auto *node = railNet->getNodeByID(linkage->nodeId);
        if (!node) return nullptr;
        projected = projectTrainNode(*node);
        break;
    }
    case Backend::NetworkKind::Truck: {
        auto *truckNet =
            qobject_cast<Backend::TruckClient::IntegrationNetwork *>(network);
        auto *node = findTruckNode(truckNet, linkage->nodeId);
        if (!node) return nullptr;
        projected = projectTruckNode(*node);
        break;
    }
    }

    QMap<QString, QVariant> props;
    props["Network_ID"] = QString::number(linkage->nodeId);

    auto *mp = new MapPoint(
        /*referencedNetworkID=*/QString::number(linkage->nodeId),
        projected, regionData->getRegion(), "circle",
        /*terminal=*/nullptr, props);
    mp->setReferenceNetwork(network);
    // Bind the view to its linkage — Task 10 added m_linkage so point
    // views carry a non-owning pointer back to the scenario model.
    mp->setLinkageModel(linkage);

    // MapPoint has no domainKey override (composite key networkName+nodeId
    // isn't a single string) → sceneRegistryKey() falls back to UUID.
    // Unchanged behavior; follow the shared rule.
    scene->addItemWithId(mp, mp->sceneRegistryKey());
    ItemEventBinder::bindMapPoint(mp, mainWindow);
    return mp;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
