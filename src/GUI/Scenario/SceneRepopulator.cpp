#include "SceneRepopulator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Commons/NetworkType.h"
#include "GUI/Controllers/NetworkDrawingController.h"
#include "GUI/Scenario/ConnectionLineFactory.h"
#include "GUI/Scenario/MapPointFactory.h"
#include "GUI/Scenario/RegionCenterPointFactory.h"
#include "GUI/Scenario/TerminalItemFactory.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QHash>
#include <QString>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

using Doc = Backend::Scenario::ScenarioDocument;

/// (networkName, nodeId) → composite key for the MapPoint lookup index
/// built in step 3 and consumed in step 5. Kept file-local so the format
/// is in one place; the index is discarded at end of repopulate so key
/// shape doesn't leak to the rest of the codebase.
QString linkageKey(const QString &networkName, int nodeId)
{
    return networkName + QLatin1Char(':') + QString::number(nodeId);
}

/// Does @p rd host this network (rail or truck)? Answered from the
/// cached name lists so we never pay the cost of lazy network
/// instantiation during scene rebuild. `RegionData::findNetworkByName`
/// would also answer this, but it materialises the network object —
/// unnecessary here since we only need a boolean.
bool regionOwnsNetwork(Backend::RegionData *rd, const QString &name)
{
    return rd && (rd->getTrainNetworks().contains(name)
                  || rd->getTruckNetworks().contains(name));
}

void rebuildRegionCenters(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    for (auto it = doc->regions.begin(); it != doc->regions.end(); ++it)
        RegionCenterPointFactory::fromRegionSpec(&it.value(), scene, mw);
}

QHash<QString, MapPoint *>
rebuildMapPoints(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    QHash<QString, MapPoint *> index;
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (!rdc) return index;

    for (auto it = doc->regions.begin(); it != doc->regions.end(); ++it)
    {
        auto *rd = rdc->getRegionData(it.key());
        if (!rd) continue;
        for (auto &link : doc->linkages)
        {
            if (!regionOwnsNetwork(rd, link.networkName)) continue;
            if (auto *mp = MapPointFactory::fromNodeLinkage(
                    &link, rd, scene, mw))
                index.insert(linkageKey(link.networkName, link.nodeId), mp);
        }
    }
    return index;
}

void rebuildTerminals(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    for (auto it = doc->terminals.begin(); it != doc->terminals.end(); ++it)
        TerminalItemFactory::fromPlacement(&it.value(), scene, mw);
}

void relinkMapPointsToTerminals(
    Doc *doc, GraphicsScene *scene,
    const QHash<QString, MapPoint *> &index)
{
    for (auto &link : doc->linkages)
    {
        if (link.excluded) continue;
        auto *mp = index.value(
            linkageKey(link.networkName, link.nodeId), nullptr);
        auto *term = scene->getItemById<TerminalItem>(link.terminalId);
        if (mp && term) mp->setLinkedTerminal(term);
    }
}

void rebuildNetworks(Doc *doc, MainWindow *mw)
{
    if (!mw || !mw->networkDrawing()) return;
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (!rdc) return;

    for (auto it = doc->regions.constBegin();
         it != doc->regions.constEnd(); ++it)
    {
        auto *rd = rdc->getRegionData(it.key());
        if (!rd) continue;

        for (auto nit = it.value().networks.constBegin();
             nit != it.value().networks.constEnd(); ++nit)
        {
            const auto &spec = nit.value();
            QString name = spec.name;
            NetworkType type;
            switch (spec.type) {
            case Backend::NetworkKind::Rail:
                type = NetworkType::Train; break;
            case Backend::NetworkKind::Truck:
                type = NetworkType::Truck; break;
            default:
                continue;
            }
            // Terminals are restored separately; skip duplicate creation.
            mw->networkDrawing()->drawNetwork(rd, type, name, true);
        }
    }
}

} // namespace

void SceneRepopulator::repopulate(Doc *doc, MainWindow *mainWindow)
{
    qCInfo(lcGuiScene) << "SceneRepopulator::repopulate: (1-arg) begin";
    if (!mainWindow) return;
    repopulate(doc, mainWindow->regionScene(),
               mainWindow->globalMapScene(), mainWindow);
}

void SceneRepopulator::repopulate(Doc           *doc,
                                  GraphicsScene *regionScene,
                                  GraphicsScene *globalScene,
                                  MainWindow    *mainWindow)
{
    qCInfo(lcGuiScene) << "SceneRepopulator::repopulate: (4-arg) begin";
    if (!doc || !regionScene) return;

    // 1. Tear down. clearAll() keeps the type-registry consistent with the
    //    underlying Qt scene; QGraphicsScene::clear() alone would leak
    //    dangling pointers via itemsByType.
    regionScene->clearAll();
    if (globalScene) globalScene->clearAll();

    // 2-5. Rebuild region scene in layered passes. Each helper is
    //      file-local (anonymous namespace) so the public interface
    //      remains a single entry point.
    rebuildRegionCenters(doc, regionScene, mainWindow);
    qCDebug(lcGuiScene) << "SceneRepopulator::repopulate:"
                        << "regions=" << doc->regions.size();
    rebuildNetworks(doc, mainWindow);
    const auto mapPointIndex =
        rebuildMapPoints(doc, regionScene, mainWindow);
    rebuildTerminals(doc, regionScene, mainWindow);
    qCDebug(lcGuiScene) << "SceneRepopulator::repopulate:"
                        << "terminals=" << doc->terminals.size();
    relinkMapPointsToTerminals(doc, regionScene, mapPointIndex);

    // 6. Connection → ConnectionLine on the region scene.
    for (auto &conn : doc->connections)
        ConnectionLineFactory::fromConnection(&conn, regionScene,
                                              mainWindow);

    qCDebug(lcGuiScene) << "SceneRepopulator::repopulate:"
                        << "connections=" << doc->connections.size();

    // 7. GlobalLink → ConnectionLine on the global scene.
    if (globalScene)
        for (auto &gl : doc->globalLinks)
            ConnectionLineFactory::fromGlobalLink(&gl, globalScene,
                                                  mainWindow);

    qCInfo(lcGuiScene) << "SceneRepopulator::repopulate: complete";
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
