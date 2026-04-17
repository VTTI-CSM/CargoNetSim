#pragma once

#include "Backend/Scenario/InterfaceConversion.h"
#include "GraphicsObjectBase.h"

#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <optional>

namespace CargoNetSim
{
namespace GUI
{

class TerminalItem;

/**
 * @brief Represents a terminal on the global map view
 *
 * A global map representation using the same pixmap as
 * TerminalItem but scaled down. Links to the original
 * terminal item and represents it on the global map.
 */
class GlobalTerminalItem : public GraphicsObjectBase
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a GlobalTerminalItem
     * @param pixmap The pixmap to display
     * @param terminalItem The linked terminal item
     * @param parent The parent item
     */
    GlobalTerminalItem(const QPixmap &pixmap,
                       TerminalItem *terminalItem = nullptr,
                       QGraphicsItem *parent = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~GlobalTerminalItem() = default;

    /**
     * @brief Get the linked terminal item
     * @return Pointer to the linked terminal item
     */
    TerminalItem *getLinkedTerminalItem() const
    {
        return linkedTerminalItem;
    }

    /**
     * @brief Set the linked terminal item
     * @param terminalItem The terminal item to link
     *
     * **Scene-registration invariant:** this item's `getID()` returns
     * the linked terminal's id when linked. Callers are expected to
     * link the terminal BEFORE adding this item to a scene via
     * `addItemWithId`. Re-linking after registration leaves the scene's
     * registry key stale — if you ever need that flow, also re-register.
     */
    void setLinkedTerminalItem(TerminalItem *terminalItem);

    /// Domain id: the linked TerminalItem's terminal id, or empty when the
    /// link isn't set or the linked terminal is unbound. No fallback to
    /// UUID — callers that want QObject identity call getID() directly.
    /// Implementation in .cpp (needs TerminalItem's full definition).
    QString getTerminalId() const;

    /// Delegates to the linked TerminalItem's typed interface accessor.
    /// Empty map when no terminal is linked. Callers get enums, not
    /// strings — consistent with TerminalItem::availableInterfaces.
    Backend::Scenario::InterfaceConversion::InterfaceMap
    availableInterfaces() const;

    /**
     * @brief Updates the item's appearance based on
     * properties from linked terminal
     */
    void updateFromLinkedTerminal();

    /**
     * @brief Compute this item's global WGS84 position from its
     *        linked TerminalItem's placement + the matching
     *        RegionCenterPoint's RegionSpec model (found in the scene).
     *
     * Formula (design spec §4):
     *   global_lon = region.globalPosition.longitude
     *              + (placement.latLon.longitude - region.localOrigin.longitude)
     *   global_lat = region.globalPosition.latitude
     *              + (placement.latLon.latitude  - region.localOrigin.latitude)
     *
     * Pure computation — kept public and separate from the projection
     * step so tests can exercise it without a live GraphicsView. Returns
     * std::nullopt when the linked terminal / placement / matching
     * RegionCenterPoint isn't reachable.
     */
    std::optional<QPointF> computeGlobalLatLon() const;

    /**
     * @brief Serializes the GlobalTerminalItem to a
     * dictionary
     * @return Dictionary containing serialized data
     */
    QMap<QString, QVariant> toDict() const;

    /**
     * @brief Creates a GlobalTerminalItem from serialized
     * data
     * @param data The serialized data dictionary
     * @param pixmap The pixmap to display
     * @param parent The parent item
     * @return Pointer to new GlobalTerminalItem instance
     */
    static GlobalTerminalItem *
    fromDict(const QMap<QString, QVariant> &data,
             const QPixmap                 &pixmap,
             QGraphicsItem *parent = nullptr);

signals:
    /**
     * @brief Signal emitted when item position has changed
     * @param newPos The new position
     */
    void positionChanged(const QPointF &newPos);

    /**
     * @brief Signal emitted when item is clicked
     * @param item The item that was clicked
     */
    void itemClicked(GlobalTerminalItem *item);

    /**
     * @brief Signal emitted when linked terminal item
     * changes
     * @param oldTerminal Previous terminal
     * @param newTerminal New terminal
     */
    void linkedTerminalChanged(TerminalItem *oldTerminal,
                               TerminalItem *newTerminal);

protected:
    /// Scene-registration hook: delegate to the linked terminal's domain id.
    QString domainKey() const override { return getTerminalId(); }

    /**
     * @brief Returns the bounding rectangle of this item
     * @return The bounding rectangle
     */
    QRectF boundingRect() const override;

    /**
     * @brief Paints the item
     * @param painter The painter to use
     * @param option Style options
     * @param widget Widget being painted on
     */
    void paint(QPainter                       *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    /**
     * @brief Handles item position changes
     * @param change The type of change
     * @param value The new value
     * @return The adjusted value
     */
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant    &value) override;

    /**
     * @brief Handles mouse hover enter events
     * @param event The hover event
     */
    void hoverEnterEvent(
        QGraphicsSceneHoverEvent *event) override;

    /**
     * @brief Handles mouse hover leave events
     * @param event The hover event
     */
    void hoverLeaveEvent(
        QGraphicsSceneHoverEvent *event) override;

    /**
     * @brief Handles mouse press events
     * @param event The mouse event
     */
    void mousePressEvent(
        QGraphicsSceneMouseEvent *event) override;

private:
    QPixmap
        originalPixmap;   ///< The original terminal pixmap
    QPixmap scaledPixmap; ///< The scaled version for global
                          ///< view
    TerminalItem
        *linkedTerminalItem; ///< The linked terminal item
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::GlobalTerminalItem)
Q_DECLARE_METATYPE(CargoNetSim::GUI::GlobalTerminalItem *)
