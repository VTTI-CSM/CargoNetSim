#pragma once

#include <QMimeData>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <Qt>
#include <variant>

namespace CargoNetSim::GUI::Input {

/**
 * Immutable record of a user input action, translated from a raw Qt event by
 * GraphicsScene / GraphicsView. Layer-1 of the dispatch pipeline (Adapter
 * pattern): isolates every other layer from QGraphicsSceneMouseEvent,
 * QKeyEvent, QMouseEvent, etc.
 *
 * Rules:
 *   - No raw Qt event pointer is stored. Translator copies the needed fields.
 *     If a handler wants something else, add a field here; do not reach through
 *     a pointer with unclear lifetime.
 *   - Coordinates are always scene-space. View-space conversion happens at the
 *     translator boundary once.
 *   - No `target` field — target resolution is the dispatcher's job; lives in
 *     ClickContext.
 */

struct PressEvent {
    Qt::MouseButton       button;
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct ReleaseEvent {
    Qt::MouseButton       button;
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct MoveEvent {
    Qt::MouseButtons      buttons;
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct DoubleClickEvent {
    Qt::MouseButton       button;
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct ContextMenuRequest {
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct WheelEvent {
    int                   angleDelta;
    Qt::KeyboardModifiers mods;
    QPointF               scenePos;
    QPoint                screenPos;
};

struct HoverEnterEvent {
    QPointF               scenePos;
};

struct HoverMoveEvent {
    QPointF               scenePos;
};

struct HoverLeaveEvent {};

struct KeyPressEvent {
    int                   key;
    Qt::KeyboardModifiers mods;
    QString               text;
    bool                  autoRepeat;
};

struct KeyReleaseEvent {
    int                   key;
    Qt::KeyboardModifiers mods;
};

struct DragEnterEvent {
    const QMimeData*      mime;
    QPointF               scenePos;
};

struct DragMoveEvent {
    const QMimeData*      mime;
    QPointF               scenePos;
};

struct DragLeaveEvent {};

struct DropEvent {
    const QMimeData*      mime;
    QPointF               scenePos;
};

using InputEvent = std::variant<
    PressEvent, ReleaseEvent, MoveEvent, DoubleClickEvent,
    ContextMenuRequest, WheelEvent,
    HoverEnterEvent, HoverMoveEvent, HoverLeaveEvent,
    KeyPressEvent, KeyReleaseEvent,
    DragEnterEvent, DragMoveEvent, DragLeaveEvent, DropEvent
>;

} // namespace CargoNetSim::GUI::Input
