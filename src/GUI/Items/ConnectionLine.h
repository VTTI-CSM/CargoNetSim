#pragma once
#include "Backend/Commons/TransportationMode.h"
#include "GraphicsObjectBase.h"

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QMap>
#include <QPen>
#include <QPropertyAnimation>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
struct Connection;
struct GlobalLink;
} // namespace Scenario
} // namespace Backend

namespace GUI
{

class TerminalItem;
class GlobalTerminalItem;
class ConnectionLabel;

class ConnectionLine : public GraphicsObjectBase
{
    Q_OBJECT

public:
    /// Visual styling per transport mode (line color/width/etc.).
    /// Keyed by the strongly-typed enum so code can't inadvertently
    /// look up the wrong case-form of a string.
    static const QMap<
        Backend::TransportationTypes::TransportationMode,
        QMap<QString, QVariant>> CONNECTION_STYLES;

    ConnectionLine(
        QGraphicsItem *startItem, QGraphicsItem *endItem,
        Backend::TransportationTypes::TransportationMode connectionType =
            Backend::TransportationTypes::TransportationMode::Truck,
        const QMap<QString, QVariant> &properties =
            QMap<QString, QVariant>(),
        const QString &region = "Default Region",
        QGraphicsItem *parent = nullptr);

    virtual ~ConnectionLine();

    void
    createAnimationVisual(const QColor &color) override;

    void clearAnimationVisuals() override;

    // Access to members
    QGraphicsItem *startItem() const
    {
        return m_startItem;
    }
    QGraphicsItem *endItem() const
    {
        return m_endItem;
    }
    Backend::TransportationTypes::TransportationMode
    connectionType() const
    {
        return m_connectionType;
    }
    int connectionId() const
    {
        return m_id;
    }
    void updateProperties(
        const QMap<QString, QVariant> &newProperties);
    const QMap<QString, QVariant> &getProperties() const
    {
        return m_properties;
    }

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
    void setConnectionType(
        Backend::TransportationTypes::TransportationMode type);
    void setProperty(const QString  &key,
                     const QVariant &value);

    /**
     * @brief Bind this line to a region-level Connection (non-owning).
     *
     * Setting the connection model clears any GlobalLink binding — a
     * ConnectionLine is either a region-local Connection OR a
     * cross-region GlobalLink, never both.
     *
     * View-only binding (Task 11). User-driven property edits that
     * should mutate the document route through ScenarioMutator at the
     * ViewController layer (Task 15/16), not here.
     *
     * Passing nullptr unbinds.
     */
    void setConnectionModel(
        Backend::Scenario::Connection *connection)
    {
        m_connection = connection;
        if (connection) m_global = nullptr;
    }

    /**
     * @brief Bind this line to a cross-region GlobalLink (non-owning).
     *        Mutually exclusive with setConnectionModel.
     */
    void setGlobalLinkModel(Backend::Scenario::GlobalLink *link)
    {
        m_global = link;
        if (link) m_connection = nullptr;
    }

    /// Non-owning connection pointer, or nullptr when unbound / in
    /// GlobalLink mode.
    Backend::Scenario::Connection *connectionModel() const
    {
        return m_connection;
    }

    /// Non-owning global-link pointer, or nullptr when unbound / in
    /// Connection mode.
    Backend::Scenario::GlobalLink *globalLinkModel() const
    {
        return m_global;
    }

    // Update methods
    void updatePosition(const QPointF &newPos  = QPointF(),
                        bool           isStart = false);

    // Selection handling
    bool isSelected() const;
    void setSelected(bool selected);

    // Serialization methods
    QMap<QString, QVariant> toDict() const;
    static ConnectionLine  *fromDict(
         const QMap<QString, QVariant>    &data,
         const QMap<int, QGraphicsItem *> &terminalsByID,
         QGraphicsScene *globalScene = nullptr,
         QGraphicsItem  *parent      = nullptr);

    /**
     * @brief Returns true when this line connects @p fromId and
     * @p toId (in either direction) via @p mode.
     */
    bool matchesConnection(
        const QString &fromId, const QString &toId,
        Backend::TransportationTypes::TransportationMode mode) const;

    /**
     * @brief Re-reads all display properties from the bound
     * backend model (Connection or GlobalLink) and schedules a
     * repaint. No-op when unbound.
     */
    void refreshFromModel();

    // Reset/set class static IDs
    static void resetClassIDs();
    static void
               setClassIDs(const QMap<int, ConnectionLine *>
                               &allConnectionsById);
    static int getNewConnectionID();

signals:
    void clicked(ConnectionLine *line);
    void startPositionChanged(const QPointF &newPos);
    void endPositionChanged(const QPointF &newPos);
    void propertyChanged(const QString  &key,
                         const QVariant &value);
    void propertiesChanged();
    void connectionTypeChanged(
        Backend::TransportationTypes::TransportationMode newType);
    void regionChanged(const QString &newRegion);

protected:
    // QGraphicsItem overrides
    QRectF       boundingRect() const override;
    QPainterPath shape() const override;
    void         paint(QPainter                       *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget = nullptr) override;
    void         mousePressEvent(
                QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(
        QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(
        QGraphicsSceneHoverEvent *event) override;

private:
    // Utility methods
    QLineF
    calculateOffsetLine(const QLineF &originalLine) const;
    void onStartItemPositionChanged(const QPointF &newPos);
    void onEndItemPositionChanged(const QPointF &newPos);
    void createConnections();
    void initializeProperties(QString region);

    // Member variables
    QGraphicsItem          *m_startItem;
    QGraphicsItem          *m_endItem;
    Backend::TransportationTypes::TransportationMode
                            m_connectionType;
    QMap<QString, QVariant> m_properties;
    int                     m_id;
    bool                    m_isHovered;

    // Geometry
    QLineF  m_line;
    QPointF m_ctrlPoint; // Control point for curved lines
    QRectF  m_boundingRect;

    // Visual
    ConnectionLabel *m_label;

    // Static members
    static int CONNECTION_LINE_ID;

    /// Non-owning pointers into ScenarioDocument. At most one is
    /// non-null at a time (mutual exclusion enforced by setters).
    Backend::Scenario::Connection *m_connection = nullptr;
    Backend::Scenario::GlobalLink *m_global     = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::ConnectionLine)
Q_DECLARE_METATYPE(CargoNetSim::GUI::ConnectionLine *)
