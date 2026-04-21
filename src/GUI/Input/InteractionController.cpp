#include "InteractionController.h"

#include "../../Backend/Commons/LogCategories.h"
#include "../../Backend/Scenario/ScenarioDocument.h"
#include "../MainWindow.h"
#include "../Widgets/GraphicsScene.h"
#include "../Widgets/GraphicsView.h"
#include "ClickContext.h"
#include "Commands/CommandBus.h"
#include "Interfaces/IClickable.h"
#include "Interfaces/IContextMenuProvider.h"
#include "Interfaces/IHoverable.h"
#include "Interfaces/IInteractionMode.h"
#include "Modes/NormalMode.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QMenu>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

namespace {

// Helper for std::visit.
template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

QString eventKindName(const InputEvent& e)
{
    return std::visit(overloaded{
        [](const PressEvent&)         { return QStringLiteral("Press"); },
        [](const ReleaseEvent&)       { return QStringLiteral("Release"); },
        [](const MoveEvent&)          { return QStringLiteral("Move"); },
        [](const DoubleClickEvent&)   { return QStringLiteral("DoubleClick"); },
        [](const ContextMenuRequest&) { return QStringLiteral("ContextMenu"); },
        [](const WheelEvent&)         { return QStringLiteral("Wheel"); },
        [](const HoverEnterEvent&)    { return QStringLiteral("HoverEnter"); },
        [](const HoverMoveEvent&)     { return QStringLiteral("HoverMove"); },
        [](const HoverLeaveEvent&)    { return QStringLiteral("HoverLeave"); },
        [](const KeyPressEvent&)      { return QStringLiteral("KeyPress"); },
        [](const KeyReleaseEvent&)    { return QStringLiteral("KeyRelease"); },
        [](const DragEnterEvent&)     { return QStringLiteral("DragEnter"); },
        [](const DragMoveEvent&)      { return QStringLiteral("DragMove"); },
        [](const DragLeaveEvent&)     { return QStringLiteral("DragLeave"); },
        [](const DropEvent&)          { return QStringLiteral("Drop"); }
    }, e);
}

QPointF eventScenePos(const InputEvent& e)
{
    return std::visit(overloaded{
        [](const PressEvent& p)         { return p.scenePos; },
        [](const ReleaseEvent& p)       { return p.scenePos; },
        [](const MoveEvent& p)          { return p.scenePos; },
        [](const DoubleClickEvent& p)   { return p.scenePos; },
        [](const ContextMenuRequest& p) { return p.scenePos; },
        [](const WheelEvent& p)         { return p.scenePos; },
        [](const HoverEnterEvent& p)    { return p.scenePos; },
        [](const HoverMoveEvent& p)     { return p.scenePos; },
        [](const HoverLeaveEvent&)      { return QPointF(); },
        [](const KeyPressEvent&)        { return QPointF(); },
        [](const KeyReleaseEvent&)      { return QPointF(); },
        [](const DragEnterEvent& p)     { return p.scenePos; },
        [](const DragMoveEvent& p)      { return p.scenePos; },
        [](const DragLeaveEvent&)       { return QPointF(); },
        [](const DropEvent& p)          { return p.scenePos; }
    }, e);
}

QPoint eventScreenPos(const InputEvent& e)
{
    return std::visit(overloaded{
        [](const PressEvent& p)         { return p.screenPos; },
        [](const ReleaseEvent& p)       { return p.screenPos; },
        [](const MoveEvent& p)          { return p.screenPos; },
        [](const DoubleClickEvent& p)   { return p.screenPos; },
        [](const ContextMenuRequest& p) { return p.screenPos; },
        [](const WheelEvent& p)         { return p.screenPos; },
        [](const auto&)                 { return QPoint(); }
    }, e);
}

Qt::KeyboardModifiers eventMods(const InputEvent& e)
{
    return std::visit(overloaded{
        [](const PressEvent& p)         { return p.mods; },
        [](const ReleaseEvent& p)       { return p.mods; },
        [](const MoveEvent& p)          { return p.mods; },
        [](const DoubleClickEvent& p)   { return p.mods; },
        [](const ContextMenuRequest& p) { return p.mods; },
        [](const WheelEvent& p)         { return p.mods; },
        [](const KeyPressEvent& p)      { return p.mods; },
        [](const KeyReleaseEvent& p)    { return p.mods; },
        [](const auto&)                 { return Qt::KeyboardModifiers(Qt::NoModifier); }
    }, e);
}

Qt::MouseButton eventButton(const InputEvent& e)
{
    return std::visit(overloaded{
        [](const PressEvent& p)       { return p.button; },
        [](const ReleaseEvent& p)     { return p.button; },
        [](const DoubleClickEvent& p) { return p.button; },
        [](const auto&)               { return Qt::NoButton; }
    }, e);
}

} // anonymous namespace

// ---------------------------------------------------------------------------

InteractionController::InteractionController(MainWindow*                          mainWindow,
                                             GraphicsScene*                       scene,
                                             GraphicsView*                        view,
                                             Backend::Scenario::ScenarioDocument* document,
                                             CommandBus*                          commandBus,
                                             QObject*                             parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_scene(scene)
    , m_view(view)
    , m_document(document)
    , m_commandBus(commandBus)
{
    Q_ASSERT(mainWindow);
    Q_ASSERT(scene);
    Q_ASSERT(view);
    Q_ASSERT(commandBus);

    // Invariant: NormalMode is always at stack bottom.
    m_modeStack.push_back(std::make_unique<NormalMode>());
    qCInfo(lcGuiInput) << "InteractionController constructed; NormalMode pushed";
}

InteractionController::~InteractionController()
{
    qCInfo(lcGuiInput) << "InteractionController destroyed; mode stack depth =" << m_modeStack.size();
}

// ---------------------------------------------------------------------------

void InteractionController::pushMode(std::unique_ptr<IInteractionMode> mode)
{
    if (!mode) {
        qCCritical(lcGuiInputMode) << "pushMode(nullptr) — invariant violation";
        return;
    }
    IInteractionMode* oldTop = currentMode();
    qCInfo(lcGuiInputMode) << "pushMode:" << oldTop->name() << "->" << mode->name()
                           << "(stack depth" << m_modeStack.size() << "->"
                           << m_modeStack.size() + 1 << ")";

    ClickContext ctx = buildContext(InputEvent{HoverLeaveEvent{}});
    oldTop->onExit(ctx);

    IInteractionMode* raw = mode.get();
    m_modeStack.push_back(std::move(mode));
    raw->onEnter(ctx);
    refreshCursor();
    emit modeChanged(raw, oldTop);
}

void InteractionController::popMode()
{
    if (m_modeStack.size() <= 1) {
        qCWarning(lcGuiInputMode) << "popMode ignored — NormalMode is at stack bottom";
        return;
    }
    IInteractionMode* oldTop = currentMode();
    const QString oldName = oldTop->name();
    ClickContext ctx = buildContext(InputEvent{HoverLeaveEvent{}});
    oldTop->onExit(ctx);

    // Detach the unique_ptr so we can log oldName safely without a dangling
    // vtable access. We also defer deletion until after onEnter / emit so that
    // modes whose onPress/onKeyPress initiates setMode<NormalMode>() (which
    // routes through popMode()) do not free themselves while still executing.
    std::unique_ptr<IInteractionMode> expiring = std::move(m_modeStack.back());
    m_modeStack.pop_back();
    IInteractionMode* newTop = currentMode();
    qCInfo(lcGuiInputMode) << "popMode:" << oldName << "->" << newTop->name()
                           << "(stack depth" << m_modeStack.size() + 1 << "->"
                           << m_modeStack.size() << ")";
    newTop->onEnter(ctx);
    refreshCursor();
    emit modeChanged(newTop, expiring.get());
    // expiring destroyed here, after all signalling is done.
}

void InteractionController::setMode(std::unique_ptr<IInteractionMode> mode)
{
    if (!mode) {
        qCCritical(lcGuiInputMode) << "setMode(nullptr) — invariant violation";
        return;
    }
    // Unwind to NormalMode, then push new mode unless the new mode IS NormalMode.
    while (m_modeStack.size() > 1) {
        popMode();
    }
    if (mode->name() == QStringLiteral("Normal")) {
        qCInfo(lcGuiInputMode) << "setMode(NormalMode) — already at normal";
        return;
    }
    pushMode(std::move(mode));
}

IInteractionMode* InteractionController::currentMode() const
{
    Q_ASSERT(!m_modeStack.empty());
    return m_modeStack.back().get();
}

// ---------------------------------------------------------------------------

GraphicsScene* InteractionController::scene() const      { return m_scene; }
GraphicsView*  InteractionController::view() const       { return m_view; }
MainWindow*    InteractionController::mainWindow() const { return m_mainWindow; }
Backend::Scenario::ScenarioDocument* InteractionController::document() const { return m_document; }

// ---------------------------------------------------------------------------

void InteractionController::setDocument(Backend::Scenario::ScenarioDocument* doc)
{
    qCInfo(lcGuiInput) << "document swap on controller" << this;
    m_document = doc;
    // NOTE: undo stack clearing is handled by MainWindow::setRuntime, not here —
    // the CommandBus is shared across both the region and global controllers, so
    // clearing per-controller would fire the clear twice (or race if order of
    // setDocument calls matters).
}

// ---------------------------------------------------------------------------

Handled InteractionController::dispatch(const InputEvent& event)
{
    ClickContext ctx = buildContext(event);
    qCDebug(lcGuiInput) << "dispatch:" << eventKindName(event)
                        << "mode=" << ctx.currentMode->name()
                        << "target=" << (void*)ctx.target;

    Handled modeVerdict = dispatchToMode(event, ctx);
    if (modeVerdict == Handled::Yes) {
        qCDebug(lcGuiInput) << "  mode consumed";
        refreshCursor();
        return Handled::Yes;
    }
    if (modeVerdict == Handled::Reject) {
        qCDebug(lcGuiInput) << "  mode rejected";
        QApplication::beep();
        if (m_mainWindow) {
            if (auto* sb = m_mainWindow->statusBar()) {
                sb->showMessage(QStringLiteral("Action not valid in current mode"), 2000);
            }
        }
        return Handled::Yes;  // caller should stop propagation
    }

    Handled itemVerdict = dispatchToItem(event, ctx);
    refreshCursor();
    return itemVerdict;
}

// ---------------------------------------------------------------------------

Handled InteractionController::dispatchToMode(const InputEvent& event, const ClickContext& ctx)
{
    IInteractionMode* mode = ctx.currentMode;
    Q_ASSERT(mode);
    return std::visit(overloaded{
        [&](const PressEvent& p)         { return mode->onPress(p, ctx); },
        [&](const ReleaseEvent& p)       { return mode->onRelease(p, ctx); },
        [&](const MoveEvent& p)          { return mode->onMove(p, ctx); },
        [&](const DoubleClickEvent& p)   { return mode->onDoubleClick(p, ctx); },
        [&](const ContextMenuRequest& p) { return mode->onContextMenu(p, ctx); },
        [&](const WheelEvent& p)         { return mode->onWheel(p, ctx); },
        [&](const HoverEnterEvent& p)    { return mode->onHoverEnter(p, ctx); },
        [&](const HoverMoveEvent& p)     { return mode->onHoverMove(p, ctx); },
        [&](const HoverLeaveEvent& p)    { return mode->onHoverLeave(p, ctx); },
        [&](const KeyPressEvent& p)      { return mode->onKeyPress(p, ctx); },
        [&](const KeyReleaseEvent& p)    { return mode->onKeyRelease(p, ctx); },
        [&](const DragEnterEvent& p)     { return mode->onDragEnter(p, ctx); },
        [&](const DragMoveEvent& p)      { return mode->onDragMove(p, ctx); },
        [&](const DragLeaveEvent& p)     { return mode->onDragLeave(p, ctx); },
        [&](const DropEvent& p)          { return mode->onDrop(p, ctx); }
    }, event);
}

Handled InteractionController::dispatchToItem(const InputEvent& event, const ClickContext& ctx)
{
    if (!ctx.target) {
        qCDebug(lcGuiInputItem) << "no target — PassThrough";
        return Handled::PassThrough;
    }

    return std::visit(overloaded{
        [&](const PressEvent& p) -> Handled {
            auto* c = dynamic_cast<IClickable*>(ctx.target);
            if (!c) return Handled::PassThrough;
            switch (p.button) {
                case Qt::LeftButton:   return c->onLeftClick  (ctx);
                case Qt::RightButton:  return c->onRightClick (ctx);
                case Qt::MiddleButton: return c->onMiddleClick(ctx);
                default:               return Handled::PassThrough;
            }
        },
        [&](const DoubleClickEvent& p) -> Handled {
            auto* c = dynamic_cast<IClickable*>(ctx.target);
            if (!c) return Handled::PassThrough;
            Q_UNUSED(p);
            return c->onDoubleClick(ctx);
        },
        [&](const ContextMenuRequest&) -> Handled {
            qCInfo(lcGuiInputItem)
                << "ContextMenuRequest: itemsUnderCursor count="
                << ctx.itemsUnderCursor.size();
            IContextMenuProvider* provider = nullptr;
            for (QGraphicsItem* item : ctx.itemsUnderCursor) {
                auto* p = dynamic_cast<IContextMenuProvider*>(item);
                qCInfo(lcGuiInputItem)
                    << "  candidate type=" << (item ? typeid(*item).name() : "null")
                    << " providesMenu=" << (p != nullptr);
                if (p) {
                    provider = p;
                    break;
                }
            }
            if (!provider) {
                qCInfo(lcGuiInputItem) << "  no IContextMenuProvider found — PassThrough";
                return Handled::PassThrough;
            }
            QMenu menu;
            provider->buildContextMenu(&menu, ctx);
            qCInfo(lcGuiInputItem) << "  menu built, actions=" << menu.actions().size();
            if (!menu.isEmpty()) {
                menu.exec(ctx.screenPos);
            }
            return Handled::Yes;
        },
        [&](const HoverEnterEvent&) -> Handled {
            if (auto* h = dynamic_cast<IHoverable*>(ctx.target)) h->onHoverEnter(ctx);
            return Handled::PassThrough;
        },
        [&](const HoverMoveEvent&) -> Handled {
            if (auto* h = dynamic_cast<IHoverable*>(ctx.target)) h->onHoverMove(ctx);
            return Handled::PassThrough;
        },
        [&](const HoverLeaveEvent&) -> Handled {
            if (auto* h = dynamic_cast<IHoverable*>(ctx.target)) h->onHoverLeave(ctx);
            return Handled::PassThrough;
        },
        [](const auto&) -> Handled { return Handled::PassThrough; }
    }, event);
}

// ---------------------------------------------------------------------------

ClickContext InteractionController::buildContext(const InputEvent& event)
{
    ClickContext ctx;
    ctx.scenePos   = eventScenePos(event);
    ctx.screenPos  = eventScreenPos(event);
    ctx.modifiers  = eventMods(event);
    ctx.button     = eventButton(event);

    ctx.target             = nullptr;
    if (m_scene) {
        // Pass the view's device transform so items with
        // ItemIgnoresTransformations (MapPoint, TerminalItem) are hit-tested
        // at their on-screen pixel size, matching what the user sees.
        // Without this, zoomed-out items are pixel-wide in scene coords and
        // items(QPointF) misses them even when the cursor is visibly on top.
        const QTransform dt = m_view ? m_view->viewportTransform() : QTransform();
        QList<QGraphicsItem*> items = m_scene->items(
            ctx.scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, dt);
        ctx.itemsUnderCursor = items;
        if (!items.isEmpty()) {
            ctx.target = items.first();
        }
    }

    ctx.scene       = m_scene;
    ctx.view        = m_view;
    ctx.mainWindow  = m_mainWindow;
    ctx.document    = m_document;
    ctx.controller  = this;
    ctx.commandBus  = m_commandBus;
    ctx.currentMode = currentMode();
    return ctx;
}

void InteractionController::refreshCursor()
{
    if (!m_view) return;
    QCursor cursor = currentMode()->cursor();
    // hoverCursor fallback handled in Plan 3 when IHoverable items exist.
    if (cursor.shape() == Qt::ArrowCursor && cursor.bitmap().isNull()) {
        // treat default-constructed cursor as "no override"
        m_view->viewport()->unsetCursor();
    } else {
        m_view->viewport()->setCursor(cursor);
    }
}

// ---------------------------------------------------------------------------

void InteractionController::selectItem(QGraphicsItem* item, bool exclusive)
{
    if (!m_scene || !item) {
        qCDebug(lcGuiInput) << "selectItem: null scene or item";
        return;
    }
    if (exclusive) m_scene->clearSelection();
    item->setSelected(true);
}

void InteractionController::clearSelection()
{
    if (!m_scene) return;
    m_scene->clearSelection();
}

} // namespace CargoNetSim::GUI::Input
