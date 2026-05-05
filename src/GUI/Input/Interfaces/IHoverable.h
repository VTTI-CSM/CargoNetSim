#pragma once

#include <QCursor>

namespace CargoNetSim::GUI::Input {

struct ClickContext;

/**
 * Items with per-hover feedback (cursor change, visual hilite). Empty
 * QCursor means "no override" — the mode's cursor or Qt default is used.
 */
class IHoverable {
public:
    virtual ~IHoverable() = default;
    virtual void    onHoverEnter(const ClickContext&) {}
    virtual void    onHoverMove (const ClickContext&) {}
    virtual void    onHoverLeave(const ClickContext&) {}
    virtual QCursor hoverCursor() const { return {}; }
};

} // namespace CargoNetSim::GUI::Input
