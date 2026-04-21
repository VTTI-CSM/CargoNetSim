#include "BackgroundPhotoItem.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QBuffer>
#include <QByteArray>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

BackgroundPhotoItem::BackgroundPhotoItem(
    const QPixmap &pixmap, const QString &regionName,
    QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , m_pixmap(pixmap)
{
    qCInfo(lcGuiScene)
        << "BackgroundPhotoItem::BackgroundPhotoItem:"
        << "region=" << regionName
        << "pixmap=" << pixmap.size();

    // Set a low Z-value to stay below other items
    setZValue(-1);

    // Initialize properties
    m_properties["Type"] =
        QString("Background - %1").arg(regionName);
    m_properties["Region"]    = regionName;
    m_properties["Scale"]     = 1.0;
    m_properties["Latitude"]  = 0.0;
    m_properties["Longitude"] = 0.0;
    m_properties["Locked"]    = false;
    m_properties["Opacity"]   = 1.0;

    // Configure flags for interaction
    setFlags(QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemIsMovable
             | QGraphicsItem::ItemSendsGeometryChanges);
}

void BackgroundPhotoItem::updateCoordinates()
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::updateCoordinates:"
        << "pos=" << pos();

    // Get scene and view
    QGraphicsView *graphicsScene = scene()->views().first();
    GraphicsView  *graphicsView =
        dynamic_cast<GraphicsView *>(graphicsScene);
    if (!graphicsView)
    {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItem::updateCoordinates:"
            << "no GraphicsView available";
        return;
    }

    auto newPos = graphicsView->sceneToWGS84(pos());

    // Update properties
    updateProperty("Latitude",
                   QString::number(newPos.y(), 'f', 6));
    updateProperty("Longitude",
                   QString::number(newPos.x(), 'f', 6));

    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::updateCoordinates:"
        << "lat=" << newPos.y() << "lon=" << newPos.x();
}

void BackgroundPhotoItem::setFromWGS84(QPointF GeoPoint)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::setFromWGS84:"
        << "geoPoint=" << GeoPoint;

    // Get scene and view
    QGraphicsScene *graphicsScene = scene();
    if (!graphicsScene || graphicsScene->views().isEmpty())
    {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItem::setFromWGS84:"
            << "no scene or views";
        return;
    }

    QGraphicsView *view = graphicsScene->views().first();
    if (!view)
    {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItem::setFromWGS84:"
            << "null view";
        return;
    }

    GraphicsView *gView =
        dynamic_cast<GraphicsView *>(view);
    if (!gView)
    {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItem::setFromWGS84:"
            << "view is not a GraphicsView";
        return;
    }

    // Convert from coordinates - call a method on the main
    // window
    QPointF scenePos = gView->wgs84ToScene(GeoPoint);

    // Set position
    setPos(scenePos);

    // Update properties
    updateProperty(
        "Latitude",
        QString::number(GeoPoint.y(), 'f', 6).toDouble());
    updateProperty(
        "Longitude",
        QString::number(GeoPoint.x(), 'f', 6).toDouble());

    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::setFromWGS84:"
        << "scenePos=" << scenePos;
}

QRectF BackgroundPhotoItem::boundingRect() const
{
    float scale = getScale();
    return QRectF(0, 0, m_pixmap.width() * scale,
                  m_pixmap.height() * scale);
}

void BackgroundPhotoItem::updateScale()
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::updateScale:"
        << "scale=" << getScale();

    prepareGeometryChange();
    update();

    // Notify about scale change
    emit scaleChanged(getScale());
}

void BackgroundPhotoItem::setRegion(const QString &region)
{
    if (m_properties["Region"] != region)
    {
        qCDebug(lcGuiScene)
            << "BackgroundPhotoItem::setRegion:"
            << "old=" << m_properties["Region"].toString()
            << "new=" << region;
        m_properties["Region"] = region;
        emit regionChanged(region);
    }
}

float BackgroundPhotoItem::getScale() const
{
    bool  ok    = false;
    float scale =
        m_properties.value("Scale", 1.0).toFloat(&ok);
    return ok ? scale : 1.0f;
}

void BackgroundPhotoItem::setScale(float scale)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::setScale:"
        << "requested=" << scale
        << "current=" << getScale();

    if (scale <= 0.0f)
    {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItem::setScale:"
            << "clamping non-positive scale" << scale
            << "to 0.1";
        scale = 0.1f; // Minimum scale
    }

    if (qAbs(scale - getScale()) > 0.001f)
    {
        updateProperty(
            "Scale",
            QString::number(scale, 'f', 2).toDouble());
        updateScale();
    }
}

qreal BackgroundPhotoItem::opacity() const
{
    return m_properties.value("Opacity", 1.0).toReal();
}

void BackgroundPhotoItem::setOpacity(qreal opacity)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::setOpacity:"
        << "requested=" << opacity;

    opacity = qBound(0.0, opacity, 1.0);

    if (qAbs(opacity
             - m_properties.value("Opacity", 1.0).toReal())
        > 0.01)
    {
        m_properties["Opacity"] = opacity;
        updateProperty(
            "Opacity",
            QString::number(opacity, 'f', 2).toDouble());

        // Must call QGraphicsItem's setOpacity which will
        // trigger a redraw
        QGraphicsItem::setOpacity(opacity);

        // Notify about opacity change
        emit opacityChanged(opacity);
    }
}

void BackgroundPhotoItem::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::paint:"
        << "pos=" << pos()
        << "scale=" << getScale();

    // Get current scale from properties
    float scale = getScale();

    // Calculate scaled dimensions
    float scaledWidth  = m_pixmap.width() * scale;
    float scaledHeight = m_pixmap.height() * scale;

    // Draw the scaled pixmap
    painter->drawPixmap(
        QRectF(0, 0, scaledWidth, scaledHeight), m_pixmap,
        QRectF(m_pixmap.rect()));

    // Draw selection rectangle if selected
    if (option->state & QStyle::State_Selected)
    {
        QPen pen(Qt::red, 2, Qt::DashLine);
        painter->setPen(pen);
        painter->drawRect(
            QRectF(0, 0, scaledWidth, scaledHeight));
    }
}

Input::Handled BackgroundPhotoItem::onLeftClick(
    const Input::ClickContext &ctx)
{
    Q_UNUSED(ctx);
    qCDebug(lcGuiInputItem)
        << "BackgroundPhotoItem::onLeftClick; locked ="
        << m_properties.value(QStringLiteral("Locked"))
               .toBool();
    return Input::Handled::PassThrough;
}

bool BackgroundPhotoItem::canDrag(
    const Input::ClickContext &ctx) const
{
    Q_UNUSED(ctx);
    return !m_properties.value(QStringLiteral("Locked"))
                .toBool();
}

QPointF BackgroundPhotoItem::onDragUpdate(
    const QPointF             &requested,
    const Input::ClickContext &ctx)
{
    Q_UNUSED(ctx);
    if (m_properties.value(QStringLiteral("Locked"))
            .toBool())
    {
        qCDebug(lcGuiInputItem)
            << "BackgroundPhotoItem: drag blocked (locked)";
        return pos();
    }
    return requested;
}

void BackgroundPhotoItem::onDragEnd(
    const QPointF             &finalPos,
    const Input::ClickContext &ctx)
{
    Q_UNUSED(ctx);
    qCDebug(lcGuiInputItem)
        << "BackgroundPhotoItem::onDragEnd; pos ="
        << finalPos;

    // Port side effect previously in itemChange(
    // ItemPositionHasChanged): refresh WGS84 coordinate
    // properties now that the drag has committed.
    if (scene())
    {
        updateCoordinates();
    }

    emit positionChanged(finalPos);
}

void BackgroundPhotoItem::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::updateProperties:"
        << "count=" << newProperties.size();

    for (auto it = newProperties.constBegin();
         it != newProperties.constEnd(); ++it)
    {
        m_properties[it.key()] = it.value();
    }
    emit propertiesChanged();
}

void BackgroundPhotoItem::updateProperty(
    const QString &key, const QVariant &value)
{
    // Only update if value actually changes
    if (m_properties.value(key) != value)
    {
        qCDebug(lcGuiScene)
            << "BackgroundPhotoItem::updateProperty:"
            << "key=" << key << "value=" << value;
        m_properties[key] = value;
        emit propertyChanged(key, value);
    }
}

QMap<QString, QVariant> BackgroundPhotoItem::toDict() const
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItem::toDict:"
        << "region=" << m_properties.value("Region").toString();

    QMap<QString, QVariant> data;

    // Store position
    QMap<QString, QVariant> posMap;
    posMap["x"]      = pos().x();
    posMap["y"]      = pos().y();
    data["position"] = posMap;

    // Store basic properties
    data["properties"]  = m_properties;
    data["selected"]    = isSelected();
    data["z_value"]     = zValue();
    data["visible"]     = isVisible();

    // Convert pixmap to base64 for serialization
    QByteArray byteArray;
    QBuffer    buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    m_pixmap.save(&buffer, "PNG");
    data["image_data"] = QString(byteArray.toBase64());

    return data;
}

BackgroundPhotoItem *BackgroundPhotoItem::fromDict(
    const QMap<QString, QVariant> &data,
    QGraphicsItem                 *parent)
{
    qCInfo(lcGuiScene)
        << "BackgroundPhotoItem::fromDict:"
        << "deserializing background photo item";

    // Convert base64 back to pixmap
    QPixmap    pixmap;
    QByteArray imageData = QByteArray::fromBase64(
        data["image_data"].toString().toLatin1());
    pixmap.loadFromData(imageData);

    QMap<QString, QVariant> prop =
        data["properties"].toMap();

    // Create new instance
    BackgroundPhotoItem *instance = new BackgroundPhotoItem(
        pixmap, prop["Region"].toString(), parent);

    // Set position
    QMap<QString, QVariant> posMap =
        data["position"].toMap();
    QPointF pos(posMap.value("x", 0).toDouble(),
                posMap.value("y", 0).toDouble());
    instance->setPos(pos);

    // Set properties
    instance->m_properties = prop;

    instance->setSelected(data["selected"].toBool());
    instance->setZValue(data["z_value"].toDouble());
    instance->setVisible(data["visible"].toBool());

    // Set opacity if present
    if (data.contains("opacity"))
    {
        instance->setOpacity(data["opacity"].toDouble());
    }

    return instance;
}

} // namespace GUI
} // namespace CargoNetSim
