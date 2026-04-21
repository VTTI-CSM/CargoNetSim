#include "RegionCenterPoint.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Input/Commands/CommandBus.h"
#include "GUI/Input/Commands/UpdateRegionLocalOriginCommand.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QMap>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

RegionCenterPoint::RegionCenterPoint(
    const QString &region, const QColor &color,
    const QMap<QString, QVariant> &properties,
    QGraphicsItem                 *parent)
    : GraphicsObjectBase(parent)
    , color(color)
    , properties(properties)
{
    qCInfo(lcGuiScene)
        << "RegionCenterPoint::RegionCenterPoint:"
        << "region=" << region
        << "color=" << color.name();

    // Set high Z-value to ensure visibility
    setZValue(100);

    // Initialize properties if none were provided
    if (this->properties.isEmpty())
    {
        this->properties = {
            {"Type", QString("Region Center")},
            {"Latitude", "0.0000000"},
            {"Longitude", "0.0000000"},
            {"Shared Latitude", "0.0000000"},
            {"Shared Longitude", "0.0000000"},
            {"Region", region}};
    }

    // Enable needed flags
    setFlags(QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemIsMovable
             | QGraphicsItem::ItemSendsGeometryChanges
             | QGraphicsItem::ItemIgnoresTransformations);

    setAcceptHoverEvents(true);
}

void RegionCenterPoint::updateCoordinates(QPointF geoPoint)
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::updateCoordinates:"
        << "geoPoint=" << geoPoint;

    QString oldLon = properties["Longitude"].toString();
    QString oldLat = properties["Latitude"].toString();

    // Format with 6 decimal places
    properties["Latitude"] =
        QString::number(geoPoint.y(), 'f', 6);
    properties["Longitude"] =
        QString::number(geoPoint.x(), 'f', 6);

    // Only emit signals if values actually changed
    if (properties["Latitude"].toString() != oldLat
        || properties["Longitude"].toString() != oldLon)
    {
        emit coordinatesChanged(geoPoint);
        emit propertyChanged("Longitude",
                             properties["Longitude"]);
        emit propertyChanged("Latitude",
                             properties["Latitude"]);
    }

    update();
}

void RegionCenterPoint::updateSharedCoordinates(
    QPointF geoPoint)
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::updateSharedCoordinates:"
        << "geoPoint=" << geoPoint;

    QString oldLon =
        properties["Shared Longitude"].toString();
    QString oldLat =
        properties["Shared Latitude"].toString();

    // Format with 6 decimal places
    properties["Shared Longitude"] =
        QString::number(geoPoint.x(), 'f', 6);
    properties["Shared Latitude"] =
        QString::number(geoPoint.y(), 'f', 6);

    // Only emit signals if values actually changed
    if (properties["Shared Latitude"].toString() != oldLat
        || properties["Shared Longitude"].toString()
               != oldLon)
    {
        emit sharedCoordinatesChanged(geoPoint);
        emit propertyChanged("Shared Latitude",
                             properties["Shared Latitude"]);
        emit propertyChanged(
            "Shared Longitude",
            properties["Shared Longitude"]);
    }

    update();
}

void RegionCenterPoint::setRegion(const QString &newRegion)
{
    if (properties.value("Region", "Default Region")
            .toString()
        != newRegion)
    {
        qCDebug(lcGuiScene)
            << "RegionCenterPoint::setRegion:"
            << "old=" << properties.value("Region").toString()
            << "new=" << newRegion;
        QString oldRegion =
            properties.value("Region", "Default Region")
                .toString();
        properties["Region"] = newRegion;
        emit regionChanged(newRegion);
    }
}

void RegionCenterPoint::setColor(const QColor &newColor)
{
    if (color != newColor)
    {
        qCDebug(lcGuiScene)
            << "RegionCenterPoint::setColor:"
            << "color=" << newColor.name();
        color = newColor;
        emit colorChanged(color);
        update();
    }
}

void RegionCenterPoint::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::updateProperties:"
        << "count=" << newProperties.size();

    for (auto it = newProperties.constBegin();
         it != newProperties.constEnd(); ++it)
    {
        properties[it.key()] = it.value();
    }
    emit propertiesChanged();
}

void RegionCenterPoint::updateCoordinatesFromPosition()
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::updateCoordinatesFromPosition:"
        << "pos=" << pos();

    QGraphicsScene *graphicsScene = scene();
    if (!graphicsScene || graphicsScene->views().isEmpty())
    {
        qCWarning(lcGuiScene)
            << "RegionCenterPoint::updateCoordinatesFromPosition:"
            << "no scene or views";
        return;
    }

    QGraphicsView *view = graphicsScene->views().first();
    if (!view)
    {
        qCWarning(lcGuiScene)
            << "RegionCenterPoint::updateCoordinatesFromPosition:"
            << "null view";
        return;
    }
    GraphicsView *viewObj =
        dynamic_cast<GraphicsView *>(view);
    if (!viewObj)
    {
        qCWarning(lcGuiScene)
            << "RegionCenterPoint::updateCoordinatesFromPosition:"
            << "view is not a GraphicsView";
        return;
    }

    QPointF result = viewObj->sceneToWGS84(pos());

    updateCoordinates(result);
}

QRectF RegionCenterPoint::boundingRect() const
{
    return QRectF(-10, -10, 20, 20);
}

void RegionCenterPoint::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::paint:"
        << "region=" << properties.value("Region").toString()
        << "pos=" << pos();

    // Draw outer circle
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(QBrush(color));
    painter->drawEllipse(-8, -8, 16, 16);

    // Draw center cross
    painter->setPen(QPen(Qt::black, 1));
    painter->drawLine(-4, 0, 4, 0);
    painter->drawLine(0, -4, 0, 4);

    // Draw selection indicator if selected
    if (option->state & QStyle::State_Selected)
    {
        painter->setPen(QPen(Qt::red, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect());
    }
}

Input::Handled RegionCenterPoint::onLeftClick(
    const Input::ClickContext &)
{
    qCDebug(lcGuiInputItem)
        << "RegionCenterPoint::onLeftClick;"
        << "region=" << properties.value("Region").toString();
    return Input::Handled::PassThrough;
}

void RegionCenterPoint::onHoverEnter(
    const Input::ClickContext &)
{
    qCDebug(lcGuiInputItem)
        << "RegionCenterPoint::onHoverEnter;"
        << "region=" << properties.value("Region").toString();
    setCursor(QCursor(Qt::PointingHandCursor));
}

void RegionCenterPoint::onHoverLeave(
    const Input::ClickContext &)
{
    qCDebug(lcGuiInputItem)
        << "RegionCenterPoint::onHoverLeave;"
        << "region=" << properties.value("Region").toString();
    unsetCursor();
}

bool RegionCenterPoint::canDrag(
    const Input::ClickContext &) const
{
    return true;
}

void RegionCenterPoint::onDragEnd(
    const QPointF             &finalPos,
    const Input::ClickContext &ctx)
{
    qCDebug(lcGuiInputItem)
        << "RegionCenterPoint::onDragEnd; pos =" << finalPos
        << "region=" << properties.value("Region").toString();

    // Port legacy itemChange side effects: refresh coordinate
    // strings from the new scene position, then notify listeners.
    updateCoordinatesFromPosition();
    emit positionChanged(finalPos);

    // Persist coordinates to backend. Prefer ctx.document; fall
    // back to the view-derived runtime document only if missing.
    if (!m_regionSpec || getRegion().isEmpty())
    {
        return;
    }

    Backend::Scenario::ScenarioDocument *doc = nullptr;
    if (ctx.document)
    {
        doc = ctx.document.data();
    }
    else
    {
        qCWarning(lcGuiInputItem)
            << "RegionCenterPoint: drag end with null document";
        return;
    }

    QPointF latLon;
    if (ctx.view)
    {
        latLon = ctx.view->sceneToWGS84(pos());
    }
    else
    {
        // No view in context — reconstruct from the updated
        // property strings written by updateCoordinatesFromPosition.
        latLon = QPointF(
            properties.value("Longitude").toString().toDouble(),
            properties.value("Latitude").toString().toDouble());
    }

    if (ctx.commandBus)
    {
        ctx.commandBus->submit(
            std::make_unique<Input::UpdateRegionLocalOriginCommand>(
                doc, getRegion(), latLon));
    }

    qCDebug(lcGuiInputItem)
        << "RegionCenterPoint: persisted coordinates"
        << getRegion() << "lat=" << latLon.y()
        << "lon=" << latLon.x();
}

QMap<QString, QVariant> RegionCenterPoint::toDict() const
{
    qCDebug(lcGuiScene)
        << "RegionCenterPoint::toDict:"
        << "region=" << properties.value("Region").toString();

    QMap<QString, QVariant> data;

    // Create position map
    QMap<QString, QVariant> posMap;
    posMap["x"] = pos().x();
    posMap["y"] = pos().y();

    // Add all data to the map
    data["position"]   = posMap;
    data["color"]      = color.name();
    data["properties"] = properties;
    data["selected"]   = isSelected();
    data["visible"]    = isVisible();
    data["z_value"]    = zValue();

    return data;
}

RegionCenterPoint *RegionCenterPoint::fromDict(
    const QMap<QString, QVariant> &data)
{
    qCInfo(lcGuiScene)
        << "RegionCenterPoint::fromDict:"
        << "region=" << data.value("region").toString();

    // Parse color from the hex string
    QColor color(data.value("color", "#000000").toString());

    QString region = data.value("region", "").toString();

    // Create new instance
    RegionCenterPoint *instance = new RegionCenterPoint(
        region, color, data.value("properties").toMap());

    // Set position
    if (data.contains("position"))
    {
        QMap<QString, QVariant> posMap =
            data["position"].toMap();
        QPointF pos(posMap.value("x", 0).toDouble(),
                    posMap.value("y", 0).toDouble());
        instance->setPos(pos);
    }

    // Set other properties
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setVisible(
        data.value("visible", true).toBool());
    instance->setZValue(
        data.value("z_value", 2).toDouble());

    return instance;
}

void RegionCenterPoint::refreshFromSpec(
    const Backend::Scenario::RegionSpec *spec)
{
    if (!spec) return;

    qCDebug(lcGuiScene)
        << "RegionCenterPoint::refreshFromSpec:"
        << "region=" << spec->name;

    properties["Latitude"]  = spec->localOrigin.latitude;
    properties["Longitude"] = spec->localOrigin.longitude;
    properties["Shared Latitude"]  =
        spec->globalPosition.latitude;
    properties["Shared Longitude"] =
        spec->globalPosition.longitude;

    if (!spec->color.isEmpty())
        color = QColor(spec->color);

    // Unidirectional data flow: localOrigin is the semantic driver of the
    // item's scene position, so any spec refresh must reposition the dot.
    // Previously this relied on Qt's drag machinery pre-moving the item,
    // which meant non-drag mutation paths (panel edits, undo/redo, file
    // load) left the pixel stuck at its old location. Completing the
    // observer here makes every doc→view sync path consistent.
    if (QGraphicsScene *s = scene();
        s && !s->views().isEmpty())
    {
        if (auto *view = dynamic_cast<GraphicsView *>(s->views().first()))
        {
            // Qt convention: x=longitude, y=latitude.
            setPos(view->wgs84ToScene(
                QPointF(spec->localOrigin.longitude,
                        spec->localOrigin.latitude)));
        }
    }

    emit propertiesChanged();
    update();
}

} // namespace GUI
} // namespace CargoNetSim
