#include "MapPointFactory.h"

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
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
        // the network-object -> canonical-name mapping remains on the
        // backend side.
        Backend::Application::NetworkViewService networkView;
        if (networkView.networkNameOf(mp->getReferenceNetwork())
            == networkName)
            return mp;
    }
    return nullptr;
}

MapPoint *MapPointFactory::fromNodeLinkage(
    Backend::Scenario::NodeLinkage *linkage,
    const QString                  &regionName,
    GraphicsScene                  *scene,
    MainWindow                     *mainWindow)
{
    if (!linkage || regionName.isEmpty() || !scene) return nullptr;

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
        << "region=" << regionName;

    Backend::Application::NetworkViewService networkView;
    const auto nodeView = networkView.resolveNodeView(
        regionName, linkage->networkName, linkage->nodeId);
    if (!nodeView || !nodeView->isValid())
    {
        qCWarning(lcGuiScene)
            << "MapPointFactory::fromNodeLinkage:"
            << "network node not found:" << linkage->networkName
            << linkage->nodeId;
        return nullptr;
    }

    QMap<QString, QVariant> props;
    props["Network_ID"] = QString::number(linkage->nodeId);

    auto *mp = new MapPoint(
        /*referencedNetworkID=*/QString::number(linkage->nodeId),
        nodeView->projectedScenePoint, regionName, "circle",
        /*terminal=*/nullptr, props);
    mp->setReferenceNetwork(nodeView->networkObject);
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
