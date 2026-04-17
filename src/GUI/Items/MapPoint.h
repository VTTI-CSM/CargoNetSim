#pragma once

#include "Backend/Scenario/TerminalPlacement.h"
#include "GraphicsObjectBase.h"

#include <QColor>
#include <QGraphicsObject>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
struct NodeLinkage;
} // namespace Scenario
} // namespace Backend

namespace GUI
{

class TerminalItem;

/**
 * @brief Represents a network point/node in the scene
 *
 * MapPoint is a visual representation of a network node
 * that can be linked to a terminal. It can have different
 * shapes and belongs to a specific network region.
 */
class MapPoint : public GraphicsObjectBase
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a new MapPoint object
     *
     * @param referencedNetworkID The network ID this point
     * references
     * @param sceneCoordinates The scene coordinates of the
     * point
     * @param region The network region this point belongs
     * to
     * @param shape The shape to draw ("circle",
     * "rectangle", "triangle")
     * @param terminal Optional terminal linked to this
     * point
     * @param properties Optional additional properties
     */
    MapPoint(const QString &referencedNetworkID,
             QPointF        sceneCoordinates,
             const QString &region,
             const QString &shape    = "circle",
             TerminalItem  *terminal = nullptr,
             const QMap<QString, QVariant> &properties =
                 QMap<QString, QVariant>());

    virtual ~MapPoint() = default;

    /**
     * @brief Bind this point to a NodeLinkage (non-owning).
     *
     * The linkage carries the canonical (networkName, nodeId, terminalId,
     * source) tuple that this point represents. When bound, callers like
     * the future ScenarioMutator delegate (Task 16) can read the linkage
     * directly instead of pulling values from m_properties.
     *
     * This is a view-only binding. `setLinkedTerminal` remains a pure
     * view call (it updates the `m_terminal` pointer + property cache
     * only); user-driven linking/unlinking that must mutate the document
     * routes through `ScenarioMutator::linkTerminalToNode` at the
     * ViewController layer (Task 16), not here.
     *
     * Passing nullptr unbinds.
     */
    void setLinkageModel(Backend::Scenario::NodeLinkage *linkage)
    {
        m_linkage = linkage;
    }

    /// Non-owning linkage pointer, or nullptr for legacy mode.
    Backend::Scenario::NodeLinkage *linkageModel() const
    {
        return m_linkage;
    }

    /**
     * @brief Sets the terminal linked to this point
     *
     * @param terminal Terminal to link to, or nullptr to
     * unlink
     */
    void setLinkedTerminal(TerminalItem *terminal);

    /**
     * @brief Sets the reference network that this point is
     * created from
     * @param network The network pointer
     */
    void setReferenceNetwork(QObject *network)
    {
        m_referenceNetwork = network;
    }

    /**
     * @brief Get the reference network that this point is
     * created from
     * @return QObject* The network pointer
     */
    QObject *getReferenceNetwork()
    {
        return m_referenceNetwork;
    }

    /**
     * @brief Sets the color of the point
     *
     * @param color The new color
     */
    void setColor(const QColor &color);

    /**
     * @brief Sets the m_region of the point
     */
    void setRegion(const QString &region)
    {
        this->m_properties["region"] = region;
    }

    /**
     * @brief Updates the properties of the point
     * @param newProperties A map of properties to update
     */
    void updateProperties(
        const QMap<QString, QVariant> &newProperties);

    /**
     * @brief Sets the coordinates of the point
     *
     * @param newPos The new scene coordinates
     */
    void setSceneCoordinate(const QPointF &newPos);

    /**
     * @brief Get the current m_terminal linked to this
     * point
     *
     * @return TerminalItem* The linked m_terminal or
     * nullptr
     */
    TerminalItem *getLinkedTerminal() const
    {
        return m_terminal;
    }

    /**
     * @brief Get the coordinates of the point
     */
    QPointF getSceneCoordinate()
    {
        return m_sceneCoordinate;
    }

    QString getReferencedNetworkNodeID() const
    {
        return m_properties.value("Network_ID", "-1")
            .toString();
    }

    /**
     * @brief Get the network m_region
     */
    QString getRegion() const
    {
        return m_properties
            .value("region", "Default Region")
            .toString();
    }

    /**
     * @brief Sets a specific property value
     * @param key The property key
     * @param value The property value to set
     */
    void setProperty(const QString  &key,
                     const QVariant &value)
    {
        if (!m_properties.contains(key)
            || m_properties[key] != value)
        {
            m_properties[key] = value;
            emit propertyChanged(key, value);
            emit propertiesChanged();
        }
    }

    /**
     * @brief Gets a specific property value
     * @param key The property key
     * @return The property value or invalid QVariant if not
     * found
     */
    QVariant getProperty(const QString &key) const
    {
        return m_properties.value(key, QVariant());
    }

    /**
     * @brief Get the m_properties map
     */
    const QMap<QString, QVariant> &getProperties() const
    {
        return m_properties;
    }

    /**
     * @brief Serializes the point to a dictionary for
     * storage
     */
    QMap<QString, QVariant> toDict() const;

    /**
     * @brief Creates a MapPoint from a serialized
     * dictionary
     */
    static MapPoint *fromDict(
        const QMap<QString, QVariant>   &data,
        const QMap<int, TerminalItem *> &terminalsById =
            QMap<int, TerminalItem *>());

signals:
    /**
     * @brief Signal emitted when the point is clicked
     */
    void clicked(MapPoint *point);

    /**
     * @brief Signal emitted when the point is moved
     */
    void positionChanged(QPointF position);

    /**
     * @brief Signal emitted when the linked terminal
     * changes
     */
    void terminalChanged(TerminalItem *oldTerminal,
                         TerminalItem *newTerminal);

    /**
     * @brief Signal emitted when the color changes
     */
    void colorChanged(const QColor &newColor);

    /**
     * @brief Signal emitted when the properties of the
     * point change
     */
    void propertiesChanged();

    /**
     * @brief Signal emitted when any property changes
     */
    void propertyChanged(const QString  &key,
                         const QVariant &value);

protected:
    QRectF boundingRect() const override;
    void   paint(QPainter                       *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget = nullptr) override;
    void   mousePressEvent(
          QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(
        QGraphicsSceneContextMenuEvent *event) override;

private:
    void
    showContextMenu(QGraphicsSceneContextMenuEvent *event);
    void createTerminalAtPosition(
        const QString &terminalType,
        Backend::Scenario::TerminalPlacement::TerminalRole role =
            Backend::Scenario::TerminalPlacement::TerminalRole::Transit);

    static int POINT_ID;

    int                     m_id;
    QPointF                 m_sceneCoordinate;
    QString                 m_shape;
    TerminalItem           *m_terminal;
    QColor                  m_color;
    QMap<QString, QVariant> m_properties;
    QObject                *m_referenceNetwork;

    /// Non-owning pointer into ScenarioDocument::linkages. When non-null
    /// this point is a VIEW of the linkage; null → legacy mode.
    Backend::Scenario::NodeLinkage *m_linkage = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::MapPoint)
Q_DECLARE_METATYPE(CargoNetSim::GUI::MapPoint *)
