#pragma once

#include "GraphicsObjectBase.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GUI/Input/Interfaces/IDraggable.h"

#include <QGraphicsObject>
#include <QMap>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QVariant>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @brief A custom QGraphicsObject for displaying background
 * photos in regions
 *
 * This class handles background images that can be
 * positioned and scaled on the scene. It maintains its own
 * properties and can be serialized/deserialized for project
 * saving.
 */
class BackgroundPhotoItem : public GraphicsObjectBase,
                            public Input::IClickable,
                            public Input::IDraggable
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    /**
     * @brief Constructor for BackgroundPhotoItem
     * @param pixmap The image to display
     * @param regionName The region this background belongs
     * to
     * @param parent Optional parent item
     */
    BackgroundPhotoItem(const QPixmap &pixmap,
                        const QString &regionName,
                        QGraphicsItem *parent = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~BackgroundPhotoItem() = default;

    /**
     * @brief Set the position using WGS84 (geographic)
     * coordinates
     * @param lon Longitude in degrees
     * @param lat Latitude in degrees
     */
    void setFromWGS84(QPointF GeoPoint);

    /**
     * @brief Update the scale based on the "Scale" property
     */
    void updateScale();

    /**
     * @brief Set the current region name
     * @param region The region name as a QString
     */
    void setRegion(const QString &region);

    /**
     * @brief Get the current region name
     * @return The region name as a QString
     */
    QString getRegion() const
    {
        return m_properties["Region"].toString();
    }

    /**
     * @brief Update the properties of the background item
     * @param newProperties A map of property key-value
     * pairs to update
     */
    void updateProperties(
        const QMap<QString, QVariant> &newProperties);

    /**
     * @brief Update a property and emit propertyChanged
     * signal
     * @param key The property key
     * @param value The new value
     */
    void updateProperty(const QString  &key,
                        const QVariant &value);

    /**
     * @brief get Properties
     * @return QMap<QString, QVariant>
     */
    QMap<QString, QVariant> getProperties() const
    {
        return m_properties;
    }

    /**
     * @brief Access the underlying pixmap.
     * @return The pixmap stored by this item.
     *
     * Exposed so commands that need to snapshot and later re-construct the
     * image (e.g., undoable deletion) can capture the raw image without
     * round-tripping through the serialized dict.
     */
    const QPixmap &pixmap() const { return m_pixmap; }

    /**
     * @brief Get the current scale factor
     * @return The scale factor as a float
     */
    float getScale() const;

    /**
     * @brief Set the scale factor
     * @param scale The new scale factor
     */
    void setScale(float scale);

    /**
     * @brief Get the opacity of the background
     * @return The opacity value (0.0 to 1.0)
     */
    qreal opacity() const;

    /**
     * @brief Set the opacity of the background
     * @param opacity The new opacity value (0.0 to 1.0)
     */
    void setOpacity(qreal opacity);

    // QGraphicsItem required overrides
    QRectF boundingRect() const override;
    void   paint(QPainter                       *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget = nullptr) override;

    // Serialization
    QMap<QString, QVariant> toDict() const;
    static BackgroundPhotoItem *
    fromDict(const QMap<QString, QVariant> &data,
             QGraphicsItem *parent = nullptr);

    // Emit a command that removes this background image from its scene and
    // the RegionDataController pointer slot, with undo that restores both.
    // Ignores the `doc` pointer — backgrounds are GUI-only overlays.
    std::unique_ptr<QUndoCommand> createDeleteCommand(
        Backend::Scenario::ScenarioDocument* doc) const override;

    // Input interface overrides
    Input::Handled
    onLeftClick(const Input::ClickContext &ctx) override;
    bool
    canDrag(const Input::ClickContext &ctx) const override;
    QPointF
    onDragUpdate(const QPointF            &requested,
                 const Input::ClickContext &ctx) override;
    void onDragEnd(const QPointF            &finalPos,
                   const Input::ClickContext &ctx) override;

signals:
    /**
     * @brief Signal emitted when the item's position
     * changes
     * @param newPos The new position in scene coordinates
     */
    void positionChanged(const QPointF &newPos);

    /**
     * @brief Signal emitted when the scale changes
     * @param scale The new scale value
     */
    void scaleChanged(float scale);

    /**
     * @brief Signal emitted when the opacity changes
     * @param opacity The new opacity value
     */
    void opacityChanged(qreal opacity);

    /**
     * @brief Signal emitted when the region name changes
     * @param region The new region name
     */
    void regionChanged(QString region);

    /**
     * @brief Signal emitted when properties change
     */
    void propertiesChanged();

    /**
     * @brief Signal emitted when any property changes
     * @param key The property key
     * @param value The new property value
     */
    void propertyChanged(const QString  &key,
                         const QVariant &value);

private:
    /**
     * @brief Update coordinate properties when position
     * changes
     */
    void updateCoordinates();

    QPixmap m_pixmap; ///< The image to display
    QMap<QString, QVariant>
        m_properties; ///< Properties map

    /// Drag-start snapshot: at `ItemPositionChange` (where onDragUpdate fires)
    /// pos() still returns the pre-drag value, so we capture it on the first
    /// onDragUpdate of a sequence. onDragEnd uses it as the old-pos for the
    /// CommandBus submission, producing a single undoable entry per drag.
    bool    m_dragInProgress = false;
    QPointF m_preDragPos;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::BackgroundPhotoItem)
Q_DECLARE_METATYPE(CargoNetSim::GUI::BackgroundPhotoItem *)
