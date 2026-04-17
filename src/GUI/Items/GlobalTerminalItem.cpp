#include "GlobalTerminalItem.h"
#include "Backend/Commons/LogCategories.h"
#include "TerminalItem.h"

#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

GlobalTerminalItem::GlobalTerminalItem(
    const QPixmap &pixmap, TerminalItem *terminalItem,
    QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , originalPixmap(pixmap)
    , linkedTerminalItem(terminalItem)
{
    qCInfo(lcGuiScene)
        << "GlobalTerminalItem::GlobalTerminalItem:"
        << "linked=" << (terminalItem ? "yes" : "no")
        << "pixmap=" << pixmap.size();

    // Enable hover events
    setAcceptHoverEvents(true);

    // Enable position change notification
    setFlags(QGraphicsItem::ItemSendsGeometryChanges
             | QGraphicsItem::ItemIgnoresTransformations
             | QGraphicsItem::ItemIsSelectable);

    // Scale down the pixmap for global map display (24x24)
    scaledPixmap =
        originalPixmap.scaled(24, 24, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);

    // Set tooltip with terminal name if available
    updateFromLinkedTerminal();
}

void GlobalTerminalItem::setLinkedTerminalItem(
    TerminalItem *terminalItem)
{
    if (linkedTerminalItem != terminalItem)
    {
        qCDebug(lcGuiScene)
            << "GlobalTerminalItem::setLinkedTerminalItem:"
            << "old=" << (linkedTerminalItem ? "valid" : "null")
            << "new=" << (terminalItem ? "valid" : "null");

        TerminalItem *oldTerminal = linkedTerminalItem;
        linkedTerminalItem        = terminalItem;

        updateFromLinkedTerminal();

        // Emit signal about terminal change
        emit linkedTerminalChanged(oldTerminal,
                                   linkedTerminalItem);
    }
}

QString GlobalTerminalItem::getTerminalId() const
{
    return linkedTerminalItem ? linkedTerminalItem->getTerminalId()
                              : QString();
}

Backend::Scenario::InterfaceConversion::InterfaceMap
GlobalTerminalItem::availableInterfaces() const
{
    return linkedTerminalItem
               ? linkedTerminalItem->availableInterfaces()
               : Backend::Scenario::InterfaceConversion::InterfaceMap{};
}

std::optional<QPointF>
GlobalTerminalItem::computeGlobalLatLon() const
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::computeGlobalLatLon: entry";

    if (!linkedTerminalItem) return std::nullopt;
    auto *placement = linkedTerminalItem->placement();
    if (!placement) return std::nullopt;

    auto *s = scene();
    if (!s) return std::nullopt;

    // Find the RegionCenterPoint whose regionSpecModel matches this
    // placement's region. O(N) over scene items — cheap enough since
    // this only runs on linked-terminal position changes, not per-tick.
    const QString &regionName = placement->region;
    for (auto *item : s->items())
    {
        auto *rcp = dynamic_cast<RegionCenterPoint *>(item);
        if (!rcp) continue;
        auto *spec = rcp->regionSpecModel();
        if (!spec || spec->name != regionName) continue;

        const double globalLon =
            spec->globalPosition.longitude
            + (placement->latLon.longitude
               - spec->localOrigin.longitude);
        const double globalLat =
            spec->globalPosition.latitude
            + (placement->latLon.latitude
               - spec->localOrigin.latitude);
        return QPointF(globalLon, globalLat);
    }
    return std::nullopt;
}

void GlobalTerminalItem::updateFromLinkedTerminal()
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::updateFromLinkedTerminal:"
        << "linked=" << (linkedTerminalItem != nullptr);

    if (linkedTerminalItem)
    {
        // When the linked terminal is bound to a placement, derive our
        // scene position from the model directly via the region's
        // globalPosition / localOrigin carried by the matching
        // RegionCenterPoint (Task 12's binding). Projection uses the
        // MainWindow's global map view.
        if (const auto globalLatLon = computeGlobalLatLon())
        {
            if (auto *s = scene())
            {
                if (auto *mw =
                        qobject_cast<MainWindow *>(s->parent()))
                {
                    if (auto *view = mw->globalMapView())
                        setPos(view->wgs84ToScene(*globalLatLon));
                }
            }
        }

        // Get terminal name for tooltip
        QString terminalName =
            linkedTerminalItem->property("Name").toString();
        if (terminalName.isEmpty())
        {
            // Try to get name from properties if not
            // available as property
            QMap<QString, QVariant> props =
                linkedTerminalItem->property("properties")
                    .toMap();
            terminalName =
                props.value("Name", "Terminal").toString();
        }

        // Update tooltip
        setToolTip(terminalName);

        // Update pixmap if needed
        QPixmap terminalPixmap =
            linkedTerminalItem->property("pixmap")
                .value<QPixmap>();
        if (!terminalPixmap.isNull()
            && terminalPixmap.cacheKey()
                   != originalPixmap.cacheKey())
        {
            originalPixmap = terminalPixmap;
            scaledPixmap   = originalPixmap.scaled(
                24, 24, Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            update(); // Trigger repaint
        }
    }
    else
    {
        setToolTip("Terminal");
    }
}

QRectF GlobalTerminalItem::boundingRect() const
{
    // Center the bounds on the pos
    return QRectF(-scaledPixmap.width() / 2,
                  -scaledPixmap.height() / 2,
                  scaledPixmap.width(),
                  scaledPixmap.height());
}

void GlobalTerminalItem::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::paint:"
        << "pos=" << pos()
        << "tooltip=" << toolTip();

    // Draw the scaled pixmap centered on the pos
    painter->drawPixmap(-scaledPixmap.width() / 2,
                        -scaledPixmap.height() / 2,
                        scaledPixmap);

    // Draw selection outline if selected
    if (option->state & QStyle::State_Selected)
    {
        QPen pen(Qt::red, 1, Qt::DashLine);
        painter->setPen(pen);
        painter->drawRect(boundingRect());
    }
}

QVariant
GlobalTerminalItem::itemChange(GraphicsItemChange change,
                               const QVariant    &value)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::itemChange:"
        << "change=" << change;

    // Emit position changed signal when position has
    // changed
    if (change == ItemPositionHasChanged)
    {
        emit positionChanged(pos());
    }

    return QGraphicsObject::itemChange(change, value);
}

void GlobalTerminalItem::hoverEnterEvent(
    QGraphicsSceneHoverEvent *event)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::hoverEnterEvent:"
        << "tooltip=" << toolTip();
    // Change cursor to hand pointer on hover
    setCursor(QCursor(Qt::PointingHandCursor));
    QGraphicsObject::hoverEnterEvent(event);
}

void GlobalTerminalItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent *event)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::hoverLeaveEvent:"
        << "tooltip=" << toolTip();
    // Reset cursor when leaving
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void GlobalTerminalItem::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::mousePressEvent:"
        << "button=" << event->button()
        << "scenePos=" << event->scenePos();

    // Emit clicked signal
    emit itemClicked(this);

    // Call base class implementation to handle selection
    // etc.
    QGraphicsObject::mousePressEvent(event);
}

QMap<QString, QVariant> GlobalTerminalItem::toDict() const
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::toDict:"
        << "tooltip=" << toolTip();

    QMap<QString, QVariant> data;

    // Store position
    QMap<QString, QVariant> posMap;
    posMap["x"]      = pos().x();
    posMap["y"]      = pos().y();
    data["position"] = posMap;

    // Store z-value, visibility and selection state
    data["z_value"]  = zValue();
    data["visible"]  = isVisible();
    data["selected"] = isSelected();
    data["tooltip"]  = toolTip();

    // Store linked terminal ID if available
    if (linkedTerminalItem)
    {
        QMap<QString, QVariant> props =
            linkedTerminalItem->property("properties")
                .toMap();
        data["linked_terminal_id"] =
            props.value("ID", QVariant());
    }

    return data;
}

GlobalTerminalItem *GlobalTerminalItem::fromDict(
    const QMap<QString, QVariant> &data,
    const QPixmap &pixmap, QGraphicsItem *parent)
{
    qCInfo(lcGuiScene)
        << "GlobalTerminalItem::fromDict:"
        << "tooltip=" << data.value("tooltip").toString();

    // Create new instance with pixmap
    GlobalTerminalItem *instance =
        new GlobalTerminalItem(pixmap, nullptr, parent);

    // Set position if available
    if (data.contains("position"))
    {
        QMap<QString, QVariant> posMap =
            data["position"].toMap();
        QPointF pos(posMap.value("x", 0).toDouble(),
                    posMap.value("y", 0).toDouble());
        instance->setPos(pos);
    }

    // Set other properties
    instance->setZValue(
        data.value("z_value", 0).toDouble());
    instance->setVisible(
        data.value("visible", true).toBool());
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setToolTip(
        data.value("tooltip", "Terminal").toString());

    // Note: The linked terminal needs to be set separately
    // after all terminals have been created, using the
    // "linked_terminal_id" stored in the data

    return instance;
}

} // namespace GUI
} // namespace CargoNetSim
