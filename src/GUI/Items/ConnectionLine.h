#pragma once
#include "Backend/Commons/TransportationMode.h"
#include "GraphicsObjectBase.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GUI/Input/Interfaces/IHoverable.h"

#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QMap>
#include <QPen>
#include <QPointer>
#include <QPropertyAnimation>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
struct Connection;
struct GlobalLink;
class  ScenarioDocument;
} // namespace Scenario
} // namespace Backend

namespace GUI
{

class TerminalItem;
class GlobalTerminalItem;
class ConnectionLabel;

class ConnectionLine : public GraphicsObjectBase,
                       public Input::IClickable,
                       public Input::IHoverable
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
     * @brief Bind this line to a region-level Connection identified by
     *        (fromTerminalId, toTerminalId, mode) in @p doc.
     *
     * Storage is the key triple — NOT a pointer into `doc->connections`.
     * That QList reshuffles element addresses on every insert/remove, so
     * any raw `Connection*` held across a mutation becomes dangling.
     * `connectionModel()` re-resolves the triple to a live pointer on
     * every call, returning nullptr if the entry is gone.
     *
     * Calling this with a non-null doc clears any prior GlobalLink
     * binding — a line is either a region Connection view OR a
     * cross-region GlobalLink view, never both.
     */
    void bindToConnection(
        Backend::Scenario::ScenarioDocument              *doc,
        const QString                                    &fromTerminalId,
        const QString                                    &toTerminalId,
        Backend::TransportationTypes::TransportationMode  mode);

    /// GlobalLink counterpart of bindToConnection. Same stable-key
    /// invariant; resolves against `doc->globalLinks`.
    void bindToGlobalLink(
        Backend::Scenario::ScenarioDocument              *doc,
        const QString                                    &fromTerminalId,
        const QString                                    &toTerminalId,
        Backend::TransportationTypes::TransportationMode  mode);

    /// Drop whichever binding is active. `connectionModel()` and
    /// `globalLinkModel()` will return nullptr until a rebind.
    void unbindModel();

    /// Resolve the stored (fromId, toId, mode) triple against the
    /// document's connections list, returning the current live Connection
    /// pointer or nullptr if the entry has been removed, the binding is a
    /// GlobalLink instead, or the doc is gone.
    Backend::Scenario::Connection *connectionModel() const;

    /// Resolve the stored triple against `doc->globalLinks`. Returns
    /// nullptr when the binding is a Connection instead, when the entry
    /// is gone, or when the doc is gone.
    Backend::Scenario::GlobalLink *globalLinkModel() const;

    /// True iff the line is bound as a region Connection view (regardless
    /// of whether the entry currently exists in the doc). Useful for
    /// filtering queries that must distinguish binding kind without
    /// triggering a lookup.
    bool isConnectionBinding() const
    {
        return m_binding == BindingKind::Connection;
    }

    /// True iff the line is bound as a GlobalLink view.
    bool isGlobalLinkBinding() const
    {
        return m_binding == BindingKind::GlobalLink;
    }

    /// The bound-from terminal id (empty when unbound).
    const QString &boundFromTerminalId() const { return m_fromId; }

    /// The bound-to terminal id (empty when unbound).
    const QString &boundToTerminalId()   const { return m_toId; }

    /// Emit a command that removes this connection from the document.
    /// Returns nullptr when the line is unbound, is a GlobalLink view
    /// (no per-line delete semantics), or when the doc pointer is
    /// absent. The command is built from the stored triple directly —
    /// never dereferences a model pointer.
    std::unique_ptr<QUndoCommand> createDeleteCommand(
        Backend::Scenario::ScenarioDocument* doc) const override;

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

    // Input interface implementations
    Input::Handled onLeftClick(const Input::ClickContext&) override;
    void onHoverEnter(const Input::ClickContext&) override;
    void onHoverLeave(const Input::ClickContext&) override;

signals:
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

    /// Stable-key binding to a ScenarioDocument entity. Raw pointers into
    /// doc->connections / doc->globalLinks are not stored because those
    /// QLists reshuffle element addresses on any mutation. Instead we
    /// keep the natural identity triple and the owning document, and
    /// resolve to a live pointer on demand.
    enum class BindingKind { Unbound, Connection, GlobalLink };
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_fromId;
    QString                                       m_toId;
    BindingKind                                   m_binding = BindingKind::Unbound;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::ConnectionLine)
Q_DECLARE_METATYPE(CargoNetSim::GUI::ConnectionLine *)
