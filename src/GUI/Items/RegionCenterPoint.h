#pragma once

#include "Backend/Scenario/RegionSpec.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GUI/Input/Interfaces/IDraggable.h"
#include "GUI/Input/Interfaces/IHoverable.h"
#include "GraphicsObjectBase.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsObject>
#include <QMap>
#include <QPointF>
#include <QVariant>

namespace CargoNetSim
{

namespace GUI
{

/**
 * @class RegionCenterPoint
 * @brief A visual representation of a region's center point
 * in the scene.
 *
 * RegionCenterPoint provides a visual indicator of a
 * region's center and holds the region's metadata,
 * including geographic coordinates and shared coordinates
 * that are used for global map positioning.
 */
class RegionCenterPoint : public GraphicsObjectBase,
                          public Input::IClickable,
                          public Input::IHoverable,
                          public Input::IDraggable
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a RegionCenterPoint.
     * @param regionName The name of the region
     * @param color The color for this region
     * @param properties Optional properties map
     * @param parent Optional parent QGraphicsItem
     */
    RegionCenterPoint(
        const QString &region, const QColor &color,
        const QMap<QString, QVariant> &properties =
            QMap<QString, QVariant>(),
        QGraphicsItem *parent = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~RegionCenterPoint() = default;

    /**
     * @brief Bind this center point to a RegionSpec (non-owning).
     *
     * View-only binding (Task 12). User-drag coordinate edits that
     * should mutate the document go through
     * `ScenarioMutator::updateRegionLocalOrigin` /
     * `updateRegionGlobalPosition` at the ViewController layer
     * (Task 16), not from inside `updateCoordinates`. Keeping the
     * mutation out of the view-level setter avoids cycles with the
     * Task 21 observer that will repopulate the scene on
     * regionChanged.
     *
     * Passing nullptr unbinds.
     */
    void setRegionSpecModel(Backend::Scenario::RegionSpec *spec)
    {
        m_regionSpec = spec;
    }

    /// Non-owning region-spec pointer, or nullptr in legacy mode.
    Backend::Scenario::RegionSpec *regionSpecModel() const
    {
        return m_regionSpec;
    }

    /// Domain id: the bound RegionSpec::name, or empty when unbound.
    /// Reads directly from the model pointer, not from the property bag
    /// (which has a human-visible fallback and is not authoritative).
    QString getRegionName() const
    {
        return m_regionSpec ? m_regionSpec->name : QString();
    }

    /**
     * @brief Refreshes all display properties from the bound
     * RegionSpec. No-op when @p spec is null.
     */
    void refreshFromSpec(
        const Backend::Scenario::RegionSpec *spec);

    /**
     * @brief Updates the region's center coordinates.
     * @param geoPoint New geodetic coordinates (long, lat)
     */
    void updateCoordinates(QPointF geoPoint);

    /**
     * @brief Updates the region's shared coordinates for
     * global mapping.
     * @param geoPoint New shared geodetic coordinates
     * (long, lat)
     */
    void updateSharedCoordinates(QPointF geoPoint);

    /**
     * @brief Set the point's newRegion
     *
     * @param newRegion New newRegion name
     */
    void setRegion(const QString &newRegion);

    /**
     * @brief Get the point's region
     *
     * @return Current region name
     */
    QString getRegion() const
    {
        return properties.value("Region", "Default Region")
            .toString();
    }

    /**
     * @brief Sets the region's color.
     * @param color New color for the region
     */
    void setColor(const QColor &color);

    /**
     * @brief set new properties
     * @param newProperties is the QMap of the new
     * properties
     */
    void updateProperties(
        const QMap<QString, QVariant> &newProperties);

    /**
     * @brief get properties
     * @return QMap<QString, QVariant> of the item
     * properties
     */
    QMap<QString, QVariant> getProperties() const
    {
        return properties;
    }

    /**
     * @brief Converts the object to a serializable
     * dictionary.
     * @return Dictionary containing all serializable data
     */
    QMap<QString, QVariant> toDict() const;

    /**
     * @brief Creates a RegionCenterPoint from dictionary
     * data.
     * @param data Dictionary containing region center point
     * data
     * @return New RegionCenterPoint instance
     */
    static RegionCenterPoint *
    fromDict(const QMap<QString, QVariant> &data);

signals:
    /**
     * @brief Signal emitted when the position changes.
     * @param newPos New position in scene coordinates
     */
    void positionChanged(const QPointF &newPos);

    /**
     * @brief Emitted when center point region changes
     *
     * @param newRegion New region name
     */
    void regionChanged(const QString &newRegion);

    /**
     * @brief Signal emitted when the coordinates change.
     * @param GeoPoint Geodetic point (long, lat)
     */
    void coordinatesChanged(QPointF GeoPoint);

    /**
     * @brief Signal emitted when the shared coordinates
     * change.
     * @param GeoPoint Geodetic point (long, lat)
     */
    void sharedCoordinatesChanged(QPointF GeoPoint);

    /**
     * @brief Signal emitted when the region color changes.
     * @param newColor The new color
     */
    void colorChanged(const QColor &newColor);

    /**
     * @brief Signal emitted when any property changes.
     * @param key Property key
     * @param value New property value
     */
    void propertyChanged(const QString  &key,
                         const QVariant &value);

    /**
     * @brief Signal emitted when all properties change.
     */
    void propertiesChanged();

protected:
    /// Scene-registration hook: region name when bound, else empty so the
    /// base's sceneRegistryKey() falls back to the auto-UUID.
    QString domainKey() const override { return getRegionName(); }

    // QGraphicsItem overrides
    QRectF boundingRect() const override;
    void   paint(QPainter                       *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget = nullptr) override;

public:
    // Input interface overrides
    Input::Handled
    onLeftClick(const Input::ClickContext &) override;
    void
    onHoverEnter(const Input::ClickContext &) override;
    void
    onHoverLeave(const Input::ClickContext &) override;
    QCursor hoverCursor() const override
    {
        return QCursor(Qt::PointingHandCursor);
    }
    bool
    canDrag(const Input::ClickContext &) const override;
    void onDragEnd(const QPointF            &finalPos,
                   const Input::ClickContext &) override;

private:
    /**
     * @brief Updates coordinates from position.
     * Called internally when position changes.
     */
    void updateCoordinatesFromPosition();

    QColor                  color;
    QMap<QString, QVariant> properties;

    /// Non-owning pointer into ScenarioDocument::regions. View-only;
    /// null → legacy mode.
    Backend::Scenario::RegionSpec *m_regionSpec = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::RegionCenterPoint)
Q_DECLARE_METATYPE(CargoNetSim::GUI::RegionCenterPoint *)
