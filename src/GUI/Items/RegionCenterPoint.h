#pragma once

#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GUI/Input/Interfaces/IDraggable.h"
#include "GUI/Input/Interfaces/IHoverable.h"
#include "GraphicsObjectBase.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsObject>
#include <QMap>
#include <QPointF>
#include <QPointer>
#include <QVariant>

namespace CargoNetSim
{

namespace Backend::Scenario { class ScenarioDocument; }

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
    /// Unique graphics-item type id. See TerminalItem::Type for rationale.
    enum { Type = UserType + 7 };
    int type() const override { return Type; }

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
     * @brief Bind this center point to its owning document + region name.
     *
     * View-only binding. The document is retained as a QPointer so it
     * cannot dangle if the runtime is swapped; the region name is the
     * lookup key into doc->regions. Rename updates the name via
     * `setRegion(newName)` — no re-binding required, no raw pointer
     * held into a relocatable container. Passing `doc == nullptr`
     * unbinds.
     */
    void setRegionBinding(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &regionName);

    /// Non-owning region-spec pointer, obtained by name lookup into
    /// the bound document each call. Never cached — returns a pointer
    /// valid only for the current call stack. nullptr when unbound or
    /// when the region name is missing from the doc.
    Backend::Scenario::RegionSpec *regionSpecModel() const;

    /// Domain name of the bound region, or empty when unbound.
    /// Reads from the authoritative property bag.
    QString getRegionName() const
    {
        return properties.value("Region").toString();
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
    // No domainKey() override on purpose: region names are mutable
    // (renameRegion), so using them as the registry key would drift
    // silently after rename — getItemById<RegionCenterPoint>(newName)
    // would return nullptr while the item sat under the old key.
    // Inheriting the base's empty-default lets sceneRegistryKey() fall
    // back to the per-item UUID, matching every other item class in
    // this codebase. Name-keyed callers use
    // RegionCenterPointFactory::findByName().

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

    /// Document this view is bound to. QPointer so a runtime swap
    /// (scenario close / reload) turns lookups into safe nullptrs
    /// rather than dangling reads. The region NAME — not a spec
    /// pointer — is the lookup key; it lives in properties["Region"]
    /// so the property bag is the single source of truth.
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::RegionCenterPoint)
Q_DECLARE_METATYPE(CargoNetSim::GUI::RegionCenterPoint *)
