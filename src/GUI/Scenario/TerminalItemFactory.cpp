#include "TerminalItemFactory.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Utils/IconCreator.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QMap>
#include <QPixmap>
#include <QPointF>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

QPointF scenePositionFor(
    const Backend::Scenario::TerminalPlacement &placement,
    MainWindow                                 *mainWindow)
{
    using PositionMode =
        Backend::Scenario::TerminalPlacement::PositionMode;

    switch (placement.mode) {
    case PositionMode::LatLon: {
        const QPointF lonLat(placement.latLon.longitude,
                             placement.latLon.latitude);
        if (mainWindow && mainWindow->regionView()) {
            return mainWindow->regionView()->wgs84ToScene(lonLat);
        }
        return lonLat;
    }
    case PositionMode::Scene:
        return QPointF(placement.scenePos.x, placement.scenePos.y);
    case PositionMode::NetworkNode:
    default:
        // NetworkNode-anchored terminals receive their scene position
        // from MapPointFactory; place at origin for now.
        return QPointF(0.0, 0.0);
    }
}

} // namespace

TerminalItem *TerminalItemFactory::fromPlacement(
    Backend::Scenario::TerminalPlacement *placement,
    GraphicsScene                        *scene,
    MainWindow                           *mainWindow)
{
    if (!placement || !scene) return nullptr;

    qCInfo(lcGuiScene)
        << "TerminalItemFactory::fromPlacement:"
        << "id=" << placement->id
        << "type=" << placement->type
        << "region=" << placement->region;

    const QMap<QString, QPixmap> icons = IconFactory::createTerminalIcons();
    const QPixmap pixmap = icons.value(placement->type);
    if (pixmap.isNull())
    {
        qCWarning(lcGuiScene)
            << "TerminalItemFactory::fromPlacement:"
            << "no icon for type:" << placement->type;
        return nullptr;
    }

    auto *item = new TerminalItem(pixmap, placement->properties,
                                  placement->region, nullptr,
                                  placement->type);
    // Bind the view to its placement. setPlacement refreshes the
    // TerminalItem's internal property snapshot from the placement,
    // so getID()/getProperty() now read through it.
    item->setPlacement(placement);
    item->setPos(scenePositionFor(*placement, mainWindow));

    // sceneRegistryKey() resolves to item->getTerminalId() (== placement->id)
    // via the base-class template method — same value as before, one rule.
    scene->addItemWithId(item, item->sceneRegistryKey());

    // Event wiring (safe no-op when mainWindow is null).
    ItemEventBinder::bindTerminalItem(item, mainWindow);

    return item;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
