#pragma once

#include <QCursor>
#include <QString>

#include "../Handled.h"
#include "../InputEvent.h"

namespace CargoNetSim::GUI::Input {

struct ClickContext;

/**
 * State-pattern base for every interaction mode (Normal, Connect, Measure,
 * LinkTerminal, UnlinkTerminal, PickDestination, GlobalPosition, Pan).
 *
 * Every handler defaults to PassThrough so subclasses override only what they
 * care about.
 */
class IInteractionMode {
public:
    virtual ~IInteractionMode() = default;

    virtual Handled onPress       (const PressEvent&,         const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onMove        (const MoveEvent&,          const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onRelease     (const ReleaseEvent&,       const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDoubleClick (const DoubleClickEvent&,   const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onContextMenu (const ContextMenuRequest&, const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onKeyPress    (const KeyPressEvent&,      const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onKeyRelease  (const KeyReleaseEvent&,    const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onWheel       (const WheelEvent&,         const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onHoverEnter  (const HoverEnterEvent&,    const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onHoverMove   (const HoverMoveEvent&,     const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onHoverLeave  (const HoverLeaveEvent&,    const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDragEnter   (const DragEnterEvent&,     const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDragMove    (const DragMoveEvent&,      const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDragLeave   (const DragLeaveEvent&,     const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDrop        (const DropEvent&,          const ClickContext&) { return Handled::PassThrough; }

    virtual void onEnter(const ClickContext&) {}
    virtual void onExit (const ClickContext&) {}

    virtual QString name() const = 0;
    virtual QCursor cursor() const { return {}; }        // empty = no override
    virtual bool    isTransient() const { return false; }
};

} // namespace CargoNetSim::GUI::Input
