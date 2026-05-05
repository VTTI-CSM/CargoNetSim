#pragma once

#include "../Handled.h"

namespace CargoNetSim::GUI::Input {

struct ClickContext;

/**
 * Strategy-pattern interface for scene items that respond to mouse button
 * presses. Right-click PRESS (onRightClick) is distinct from the context-menu
 * request (handled by IContextMenuProvider) — Qt fires both; most items want
 * the menu and leave onRightClick as PassThrough.
 */
class IClickable {
public:
    virtual ~IClickable() = default;
    virtual Handled onLeftClick   (const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onRightClick  (const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onMiddleClick (const ClickContext&) { return Handled::PassThrough; }
    virtual Handled onDoubleClick (const ClickContext&) { return Handled::PassThrough; }
};

} // namespace CargoNetSim::GUI::Input
