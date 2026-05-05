#pragma once

#include <QPointF>

namespace CargoNetSim::GUI::Input {

struct ClickContext;

/**
 * Items that persist position changes. onDragUpdate is called for every pixel
 * (Qt itemChange route) and may clamp/snap. onDragEnd fires ONCE at release
 * after a drag is detected — this is where the command is submitted so
 * scenario mutation happens once per drag gesture, not per pixel.
 */
class IDraggable {
public:
    virtual ~IDraggable() = default;
    virtual bool    canDrag(const ClickContext&) const           { return true; }
    virtual QPointF onDragUpdate(const QPointF& requested, const ClickContext&) { return requested; }
    virtual void    onDragEnd(const QPointF& finalPos, const ClickContext&) {}
};

} // namespace CargoNetSim::GUI::Input
