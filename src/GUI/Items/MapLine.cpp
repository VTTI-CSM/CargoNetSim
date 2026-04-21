#include "MapLine.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Items/AnimationObject.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

MapLine::MapLine(const QString &referenceNetworkID,
                 const QPointF &startPoint,
                 const QPointF &endPoint,
                 const QString &region,
                 const QMap<QString, QVariant> &properties)
    : GraphicsObjectBase(nullptr)
    , m_referenceNetwork(nullptr)
    , startPoint(startPoint)
    , endPoint(endPoint)
    , m_properties(properties)
    , baseWidth(1)
    , pen(Qt::black, baseWidth)
{
    qCInfo(lcGuiScene)
        << "MapLine::MapLine:"
        << "networkID=" << referenceNetworkID
        << "region=" << region
        << "start=" << startPoint
        << "end=" << endPoint;

    this->m_properties["Network_ID"] = referenceNetworkID;
    this->m_properties["region"]     = region;

    // Set higher z-value to ensure lines are drawn above
    // the background but below points
    setZValue(3);

    // Make the line selectable
    setFlags(QGraphicsItem::ItemIsSelectable);
}

void MapLine::setColor(const QColor &color)
{
    if (pen.color() != color)
    {
        qCDebug(lcGuiScene)
            << "MapLine::setColor:"
            << "color=" << color.name();
        pen.setColor(color);
        emit colorChanged(color);
        update();
    }
}

void MapLine::setPen(const QPen &newPen)
{
    if (pen != newPen)
    {
        qCDebug(lcGuiScene)
            << "MapLine::setPen:"
            << "color=" << newPen.color().name()
            << "width=" << newPen.width();
        QColor oldColor = pen.color();
        pen             = newPen;

        if (pen.color() != oldColor)
        {
            emit colorChanged(pen.color());
        }

        update();
    }
}

void MapLine::setPoints(const QPointF &newStartPoint,
                        const QPointF &newEndPoint)
{
    qCDebug(lcGuiScene)
        << "MapLine::setPoints:"
        << "start=" << newStartPoint
        << "end=" << newEndPoint;
    startPoint = newStartPoint;
    endPoint   = newEndPoint;
    update();
}

QRectF MapLine::boundingRect() const
{
    // Calculate bounding rect with padding
    return QRectF(qMin(startPoint.x(), endPoint.x()),
                  qMin(startPoint.y(), endPoint.y()),
                  qAbs(endPoint.x() - startPoint.x()),
                  qAbs(endPoint.y() - startPoint.y()))
        .adjusted(-2, -2, 2, 2);
}

QPainterPath MapLine::shape() const
{
    // Default shape() returns the bounding rect, which for a diagonal line
    // covers a large rectangular region that steals clicks from items
    // rendered beneath the line (e.g. MapPoint nodes, TerminalItems). We
    // return a narrow stroked path along the line itself so hit-testing
    // matches what the user sees.
    QPainterPath line;
    line.moveTo(startPoint);
    line.lineTo(endPoint);

    QPainterPathStroker stroker;
    // Hit tolerance in scene units. The visual pen width comes from paint()
    // and scales with zoom; we use a fixed tolerance here since shape() is
    // consulted in item-local (== scene) coordinates for MapLine.
    stroker.setWidth(8.0);
    stroker.setCapStyle(Qt::FlatCap);
    return stroker.createStroke(line);
}

void MapLine::paint(QPainter                       *painter,
                    const QStyleOptionGraphicsItem *option,
                    QWidget                        *widget)
{
    qCDebug(lcGuiScene)
        << "MapLine::paint:"
        << "start=" << startPoint
        << "end=" << endPoint;

    // Get the current view scale
    QGraphicsScene *itemScene = scene();
    if (!itemScene || itemScene->views().isEmpty())
    {
        return;
    }

    QGraphicsView *view      = itemScene->views().first();
    qreal          viewScale = view->transform().m11();

    // Scale the pen width inversely to maintain constant
    // visual thickness
    QPen scaledPen(pen);
    scaledPen.setWidth(
        qMax(1, qRound(baseWidth / viewScale)));

    // Change pen style if selected
    if (option->state & QStyle::State_Selected)
    {
        scaledPen.setColor(Qt::blue);
        scaledPen.setStyle(Qt::DashLine);
    }

    painter->setPen(scaledPen);
    painter->drawLine(startPoint, endPoint);
}

Input::Handled
MapLine::onLeftClick(const Input::ClickContext &ctx)
{
    const QString regionName =
        m_properties.value("region").toString();
    const QString networkName =
        m_properties.value("Network_ID").toString();

    qCDebug(lcGuiInputItem)
        << "MapLine::onLeftClick; region =" << regionName
        << "network =" << networkName;

    if (!ctx.scene)
    {
        return Input::Handled::PassThrough;
    }

    ctx.scene->clearSelection();
    for (QGraphicsItem *item : ctx.scene->items())
    {
        if (auto *m = dynamic_cast<MapLine *>(item))
        {
            if (m->getRegion() == regionName
                && m->getReferencedNetworkLinkID()
                       == networkName)
            {
                m->setSelected(true);
            }
        }
    }
    return Input::Handled::Yes;
}

QMap<QString, QVariant> MapLine::toDict() const
{
    qCDebug(lcGuiScene)
        << "MapLine::toDict:"
        << "networkID=" << m_properties.value("Network_ID").toString();

    QMap<QString, QVariant> data;
    data["referenced_network_ID"] =
        m_properties.value("Network_ID");

    // Convert points to dictionaries
    QMap<QString, QVariant> startPointDict;
    startPointDict["x"] = startPoint.x();
    startPointDict["y"] = startPoint.y();

    QMap<QString, QVariant> endPointDict;
    endPointDict["x"] = endPoint.x();
    endPointDict["y"] = endPoint.y();

    data["start_point"] = startPointDict;
    data["end_point"]   = endPointDict;
    data["properties"]  = m_properties;
    data["color"]       = pen.color().name();
    data["selected"]    = isSelected();
    data["z_value"]     = zValue();
    data["base_width"]  = baseWidth;

    return data;
}

MapLine *
MapLine::fromDict(const QMap<QString, QVariant> &data)
{
    qCInfo(lcGuiScene)
        << "MapLine::fromDict:"
        << "networkID=" << data.value("referenced_network_ID").toString();

    // Extract start and end points
    QMap<QString, QVariant> startPointDict =
        data["start_point"].toMap();
    QMap<QString, QVariant> endPointDict =
        data["end_point"].toMap();

    qreal startX = startPointDict["x"].toDouble();
    qreal startY = startPointDict["y"].toDouble();
    qreal endX   = endPointDict["x"].toDouble();
    qreal endY   = endPointDict["y"].toDouble();

    MapLine *instance = new MapLine(
        data.value("referenced_network_ID").toString(),
        QPointF(startX, startY), QPointF(endX, endY),
        data.value("region", "default").toString(),
        data.value("properties").toMap());

    // Set color and other properties
    instance->setColor(
        QColor(data.value("color", "#000000").toString()));
    instance->baseWidth =
        data.value("base_width", 5).toInt();
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setZValue(
        data.value("z_value", 3).toDouble());

    return instance;
}

void MapLine::clearAnimationVisuals()
{
    GraphicsObjectBase::clearAnimationVisuals();
}

void MapLine::createAnimationVisual(const QColor &color)
{
    qCDebug(lcGuiScene)
        << "MapLine::createAnimationVisual:"
        << "color=" << color.name();

    // Create a path item as an overlay
    QPainterPath path;
    path.moveTo(startPoint);
    path.lineTo(endPoint);

    QGraphicsView *view =
        scene() && !scene()->views().isEmpty()
            ? scene()->views().first()
            : nullptr;
    qreal viewScale = view ? view->transform().m11() : 1.0;
    qreal penWidth  = qMax(5.0, 6.0 / viewScale);

    QGraphicsPathItem *overlay =
        new QGraphicsPathItem(path);
    overlay->setPen(QPen(color, penWidth, Qt::SolidLine));
    overlay->setZValue(100);

    // Important: Don't set the parent in the constructor
    overlay->setParentItem(this);

    m_animObject->setOverlay(overlay);
}

} // namespace GUI
} // namespace CargoNetSim
