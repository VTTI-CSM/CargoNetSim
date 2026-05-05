#include "ItemEventBinder.h"

#include "Backend/Commons/LogCategories.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Controllers/TerminalController.h"
#include "GUI/Items/BackgroundPhotoItem.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/PropertiesPanel.h"

#include <QObject>
#include <QPointF>

// Post Plan-3 refactor: items no longer expose per-type `clicked` signals
// — selection routes through InteractionController and PropertiesPanel
// listens to `scene->selectionChanged` instead. This file now wires only
// notification-level signals (position changes, coordinate edits, etc.).

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

int ItemEventBinder::bindTerminalItem(TerminalItem *item,
                                      MainWindow   *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindTerminalItem:"
        << "terminalId=" << item->getTerminalId();

    QObject::connect(
        item, &TerminalItem::positionChanged,
        [mainWindow, item]() {
            mainWindow->terminalCtrl()->updateGlobalMapItem(item);
        });
    return 1;
}

int ItemEventBinder::bindMapPoint(MapPoint *item, MainWindow *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindMapPoint:"
        << "pointId=" << item->getID();

    // Nothing to wire — click-to-panel handled via scene selection now.
    return 0;
}

int ItemEventBinder::bindConnectionLine(ConnectionLine *line,
                                        MainWindow     *mainWindow)
{
    if (!line || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindConnectionLine:"
        << "lineId=" << line->getID();

    // Click-to-panel removed — handled via scene selection.
    return 0;
}

int ItemEventBinder::bindGlobalTerminalItem(GlobalTerminalItem *item,
                                            MainWindow *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindGlobalTerminalItem:"
        << "terminalId=" << item->getTerminalId();

    return 0;
}

int ItemEventBinder::bindRegionCenterPoint(RegionCenterPoint *item,
                                           MainWindow        *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindRegionCenterPoint:"
        << "region=" << item->getRegion();

    // Drag / edit → refresh the region's global-map mirror and keep the
    // panel's coordinate fields in sync with the live position.
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

    return 2;
}

int ItemEventBinder::bindBackgroundPhotoItem(BackgroundPhotoItem *item,
                                             MainWindow          *mainWindow)
{
    if (!item || !mainWindow) return 0;

    qCDebug(lcGuiScene)
        << "ItemEventBinder::bindBackgroundPhotoItem:"
        << "region=" << item->getRegion();

    // BackgroundPhoto's panel surfaces Latitude/Longitude, not raw X/Y. On
    // every position change, refresh those coordinate fields from the view's
    // scene→WGS84 transform. Guarded to the case where the panel is currently
    // showing this item — same pattern used for TerminalItem/RegionCenterPoint.
    QObject::connect(
        item, &BackgroundPhotoItem::positionChanged, mainWindow,
        [mainWindow, item](const QPointF &pos) {
            auto *panel = mainWindow->propertiesPanel();
            if (!panel || panel->getCurrentItem() != item) return;

            GraphicsView *view =
                (item->getRegion() == QStringLiteral("global"))
                    ? mainWindow->globalMapView()
                    : mainWindow->regionView();
            if (!view) return;
            panel->updateCoordinateFields(view->sceneToWGS84(pos));
        });
    return 1;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
