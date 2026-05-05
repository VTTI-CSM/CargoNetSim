#pragma once

#include "GUI/Items/AnimationObject.h"
#include "Backend/Commons/TransportationMode.h"
#include <QColor>
#include <QGraphicsObject>
#include <QGraphicsRectItem>
#include <QPointer>
#include <QPropertyAnimation>
#include <memory>

class QUndoCommand;

namespace CargoNetSim
{
namespace Backend::Scenario { class ScenarioDocument; }

namespace GUI
{

class GraphicsScene;
namespace Input {
class InteractionController;
struct ClickContext;
}

class GraphicsObjectBase : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit GraphicsObjectBase(
        QGraphicsItem *parent = nullptr);
    ~GraphicsObjectBase() override;

    /// QObject instance identity. UUID assigned at construction, stable
    /// for the lifetime of the item. Non-virtual: "which object is this?"
    /// has exactly one answer. Callers that want a document-addressable
    /// id should use the domain accessor (e.g. TerminalItem::getTerminalId)
    /// or sceneRegistryKey() — not getID().
    QString getID() const;

    /// Scene-registration contract. Returns the item's domain key when the
    /// item is bound to a model entity; otherwise falls back to `getID()`
    /// (UUID). This is the single rule every `addItemWithId` /
    /// `removeItemWithId` call site should honour — the fallback lives here
    /// so it isn't duplicated at every site.
    QString sceneRegistryKey() const
    {
        const QString d = domainKey();
        return d.isEmpty() ? getID() : d;
    }

    /** Pulsing highlight effect */
    void flash(bool          evenIfHidden = false,
               const QColor &color = QColor(255, 0, 0,
                                            180));

    /// Lookup via the scene's stored pointer; nullptr if item has not been
    /// added to a GraphicsScene.
    Input::InteractionController* interactionController() const;

    /// Produce a QUndoCommand that removes this item from whatever owns it
    /// (document, scene, controller state), or nullptr if the item cannot or
    /// should not be deleted via the user-delete path. Each concrete type
    /// owns its own removal semantics — callers (NormalMode::onKeyPress,
    /// future delete sites) must NOT dynamic_cast per type. `doc` may be
    /// null for items whose deletion does not touch the ScenarioDocument.
    virtual std::unique_ptr<QUndoCommand> createDeleteCommand(
        Backend::Scenario::ScenarioDocument* doc) const;

    /// Which item's deletion this one logically triggers. Default is `this`.
    /// Child-decoration items (e.g., ConnectionLabel) override to return their
    /// owning parent so "delete the label" means "delete the parent line".
    /// NormalMode uses this to collapse selections to unique targets before
    /// requesting commands — a selection containing both a label and its
    /// parent produces one command, not two.
    virtual const GraphicsObjectBase* deleteTarget() const { return this; }

    /// Produce a QUndoCommand that creates a "connect" relationship from
    /// this item to @p other via @p mode. Each concrete item type owns what
    /// "connect" means for it: a region TerminalItem emits a region
    /// Connection command; a GlobalTerminalItem emits a cross-region
    /// GlobalLink command. Returns nullptr for unsupported pairs (mismatched
    /// item kinds, unbound endpoints, missing doc, etc.) so ConnectMode
    /// needs no per-type dispatch.
    virtual std::unique_ptr<QUndoCommand> createConnectCommandTo(
        const GraphicsObjectBase*                        other,
        Backend::TransportationTypes::TransportationMode mode,
        Backend::Scenario::ScenarioDocument*             doc) const;

protected:
    /// Hook for derived classes that view a document entity. Return the
    /// model-field identity (e.g., TerminalPlacement::id, RegionSpec::name)
    /// when bound; return an empty QString when unbound. Base returns
    /// empty — classes that don't represent a document entity inherit
    /// "no domain key".
    virtual QString domainKey() const { return QString(); }

    /** Clear any overlay visuals */
    virtual void clearAnimationVisuals();
    /** Create the overlay */
    virtual void createAnimationVisual(const QColor &color);
    void         onAnimationFinished();

    // Architecture invariant — subclasses MUST respond via
    // IClickable / IContextMenuProvider / IHoverable / IDraggable,
    // NEVER by overriding these Qt hooks. The compiler enforces this:
    // every mouse / hover / context-menu / itemChange entry point is
    // finalized here and dispatches through the interface registry.
    void     mousePressEvent      (QGraphicsSceneMouseEvent*)       final;
    void     mouseMoveEvent       (QGraphicsSceneMouseEvent*)       final;
    void     mouseReleaseEvent    (QGraphicsSceneMouseEvent*)       final;
    void     mouseDoubleClickEvent(QGraphicsSceneMouseEvent*)       final;
    void     contextMenuEvent     (QGraphicsSceneContextMenuEvent*) final;
    void     hoverEnterEvent      (QGraphicsSceneHoverEvent*)       final;
    void     hoverMoveEvent       (QGraphicsSceneHoverEvent*)       final;
    void     hoverLeaveEvent      (QGraphicsSceneHoverEvent*)       final;
    QVariant itemChange(GraphicsItemChange change,
                        const QVariant&   value)                    final;

    AnimationObject    *m_animObject = nullptr;
    QPropertyAnimation *m_animation  = nullptr;

private:
    /// Build a minimal ClickContext from scene/view/controller lookups.
    /// Used by itemChange / mouseReleaseEvent — Qt event synthesis paths
    /// that don't flow through InteractionController::buildContext().
    Input::ClickContext buildMinimalContext() const;

    QString                                         m_id;
    mutable QPointer<Input::InteractionController>  m_cachedController;
    bool                                            m_draggingSincePress = false;
};

} // namespace GUI
} // namespace CargoNetSim
