#include "SceneRepopulator.h"

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Commons/NetworkType.h"
#include "GUI/Controllers/NetworkDrawingController.h"
#include "GUI/Controllers/TerminalController.h"
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

void rebuildRegionCenters(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    for (auto it = doc->regions.begin(); it != doc->regions.end(); ++it)
        RegionCenterPointFactory::fromRegionSpec(&it.value(), scene, mw);
}

QHash<QString, MapPoint *>
rebuildMapPoints(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    QHash<QString, MapPoint *> index;
    Backend::Application::NetworkViewService networkView;

    for (auto it = doc->regions.begin(); it != doc->regions.end(); ++it)
    {
        for (auto &link : doc->linkages)
        {
            if (!networkView.regionOwnsNetwork(
                    it.key(), link.networkName))
            {
                continue;
            }
            if (auto *mp = MapPointFactory::fromNodeLinkage(
                    &link, it.key(), scene, mw))
                index.insert(linkageKey(link.networkName, link.nodeId), mp);
        }
    }
    return index;
}

void rebuildTerminals(Doc *doc, GraphicsScene *scene, MainWindow *mw)
{
    for (auto it = doc->terminals.begin(); it != doc->terminals.end(); ++it)
    {
        auto *item = TerminalItemFactory::fromPlacement(&it.value(), scene, mw);
        if (item && mw && mw->terminalCtrl())
            mw->terminalCtrl()->updateGlobalMapItem(item);
    }
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

    for (auto it = doc->regions.constBegin();
         it != doc->regions.constEnd(); ++it)
    {
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
            mw->networkDrawing()->drawNetwork(
                it.key(), type, name, true);
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
        ConnectionLineFactory::fromConnection(doc, &conn, regionScene,
                                              mainWindow);

    qCDebug(lcGuiScene) << "SceneRepopulator::repopulate:"
                        << "connections=" << doc->connections.size();

    // 7. GlobalLink → ConnectionLine on the global scene.
    if (globalScene)
        for (auto &gl : doc->globalLinks)
            ConnectionLineFactory::fromGlobalLink(doc, &gl, globalScene,
                                                  mainWindow);

    qCInfo(lcGuiScene) << "SceneRepopulator::repopulate: complete";
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
