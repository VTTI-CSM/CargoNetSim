#include "ItemEventBinder.h"

#include "Backend/Commons/LogCategories.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Controllers/TerminalController.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/PropertiesPanel.h"

#include <QObject>
#include <QPointF>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

template <typename ItemT>
void connectClickedToPanel(ItemT *item, MainWindow *mainWindow)
{
    QObject::connect(
        item, &ItemT::clicked,
        [mainWindow](ItemT *it) {
            UtilitiesFunctions::updatePropertiesPanel(mainWindow, it);
        });
}

template <typename ItemT>
int connectClickAsTerminalOrNode(ItemT *item, MainWindow *mainWindow)
{
    connectClickedToPanel(item, mainWindow);
    QObject::connect(item, &ItemT::clicked, mainWindow,
                     &MainWindow::handleTerminalNodeLinking);
    QObject::connect(item, &ItemT::clicked, mainWindow,
                     &MainWindow::handleTerminalNodeUnlinking);
    return 3;
}

} // namespace

int ItemEventBinder::bindTerminalItem(TerminalItem *item,
                                      MainWindow   *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindTerminalItem:"
        << "terminalId=" << item->getTerminalId();

    int connected = connectClickAsTerminalOrNode(item, mainWindow);

    QObject::connect(
        item, &TerminalItem::positionChanged,
        [mainWindow, item]() {
            mainWindow->terminalCtrl()->updateGlobalMapItem(item);
        });
    ++connected;

    return connected;
}

int ItemEventBinder::bindMapPoint(MapPoint *item, MainWindow *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindMapPoint:"
        << "pointId=" << item->getID();

    return connectClickAsTerminalOrNode(item, mainWindow);
}

int ItemEventBinder::bindConnectionLine(ConnectionLine *line,
                                        MainWindow     *mainWindow)
{
    if (!line || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindConnectionLine:"
        << "lineId=" << line->getID();

    connectClickedToPanel(line, mainWindow);
    return 1;
}

int ItemEventBinder::bindGlobalTerminalItem(GlobalTerminalItem *item,
                                            MainWindow *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindGlobalTerminalItem:"
        << "terminalId=" << item->getTerminalId();

    // No MainWindow-slot connections today; pattern uniformity only.
    return 0;
}

int ItemEventBinder::bindRegionCenterPoint(RegionCenterPoint *item,
                                           MainWindow        *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindRegionCenterPoint:"
        << "region=" << item->getRegion();

    // Click → PropertiesPanel: same helper every other item uses.
    connectClickedToPanel(item, mainWindow);

    // Drag / edit → refresh the region's global-map mirror and keep the
    // panel's coordinate fields in sync with the live position. Legacy
    // path used to inline these three lambdas in
    // `RegionController::createRegionCenter`; centralising here so both
    // factory and legacy callers wire the same handlers.
    const QString regionName = item->getRegion();
    QObject::connect(
        item, &RegionCenterPoint::coordinatesChanged, mainWindow,
        [mainWindow, regionName](QPointF newGeopoint) {
            UtilitiesFunctions::updateGlobalMapForRegion(
                mainWindow, regionName);
            if (auto *panel = mainWindow->propertiesPanel())
                panel->updateCoordinateFields(newGeopoint);
        });
    QObject::connect(
        item, &RegionCenterPoint::propertiesChanged, mainWindow,
        [mainWindow, regionName]() {
            UtilitiesFunctions::updateGlobalMapForRegion(
                mainWindow, regionName);
        });

    return 3;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
