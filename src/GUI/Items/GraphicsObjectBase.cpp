#include "GraphicsObjectBase.h"
#include "AnimationObject.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Input/InteractionController.h"
#include "GUI/Input/Interfaces/IDraggable.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QBrush>
#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLoggingCategory>
#include <QPen>
#include <QPropertyAnimation>
#include <QUuid>

using namespace CargoNetSim::GUI;

GraphicsObjectBase::GraphicsObjectBase(
    QGraphicsItem *parent)
    : QGraphicsObject{parent}
    , m_animObject{new AnimationObject(this)}
    , m_animation{new QPropertyAnimation(m_animObject,
                                         "opacity", this)}
    , m_id{QUuid::createUuid().toString(
          QUuid::WithoutBraces)}
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::GraphicsObjectBase:"
        << "id=" << m_id;

    // 4. Auto-delete when finished
    m_animation->setDuration(1000);
    m_animation->setLoopCount(3);
    m_animation->setStartValue(1.0);
    m_animation->setKeyValueAt(0.5, 0.0);
    m_animation->setEndValue(1.0);

    connect(m_animation, &QPropertyAnimation::finished,
            this, &GraphicsObjectBase::onAnimationFinished);
}

GraphicsObjectBase::~GraphicsObjectBase() = default;

QString GraphicsObjectBase::getID() const
{
    return m_id;
}

std::unique_ptr<QUndoCommand> GraphicsObjectBase::createDeleteCommand(
    Backend::Scenario::ScenarioDocument* /*doc*/) const
{
    // Default: item has no user-deletable semantics. Concrete types override.
    return nullptr;
}

std::unique_ptr<QUndoCommand> GraphicsObjectBase::createConnectCommandTo(
    const GraphicsObjectBase*                         /*other*/,
    Backend::TransportationTypes::TransportationMode  /*mode*/,
    Backend::Scenario::ScenarioDocument*              /*doc*/) const
{
    // Default: item has no user-connectable semantics. TerminalItem and
    // GlobalTerminalItem override to emit the appropriate command.
    return nullptr;
}

void GraphicsObjectBase::flash(bool          evenIfHidden,
                               const QColor &color)
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::flash:"
        << "id=" << m_id
        << "evenIfHidden=" << evenIfHidden
        << "color=" << color.name();

    bool wasHidden = !isVisible();
    if (evenIfHidden && wasHidden)
    {
        setVisible(true);
    }

    m_animObject->setWasHidden(wasHidden);
    m_animObject->setRestoreVisibility(evenIfHidden
                                       && wasHidden);

    if (m_animation->state() != QAbstractAnimation::Stopped)
    {
        m_animation->stop();
    }

    clearAnimationVisuals();
    createAnimationVisual(color);

    m_animation->start();
}

void GraphicsObjectBase::clearAnimationVisuals()
{
    m_animObject->clearVisuals();
}

void GraphicsObjectBase::createAnimationVisual(
    const QColor &color)
{
    auto *rect =
        new QGraphicsRectItem(boundingRect(), this);
    rect->setBrush(QBrush(color));
    rect->setPen(QPen(Qt::NoPen));
    rect->setZValue(100);
    m_animObject->setRect(rect);
}

void GraphicsObjectBase::onAnimationFinished()
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::onAnimationFinished:"
        << "id=" << m_id;
    clearAnimationVisuals();
    if (m_animObject->shouldRestoreVisibility())
        setVisible(false);
}

Input::InteractionController*
GraphicsObjectBase::interactionController() const
{
    if (m_cachedController) return m_cachedController.data();
    if (auto* gs = qobject_cast<GraphicsScene*>(scene())) {
        m_cachedController = gs->inputController();
        return m_cachedController.data();
    }
    return nullptr;
}

QVariant GraphicsObjectBase::itemChange(
    GraphicsItemChange change, const QVariant& value)
{
    switch (change) {
    case ItemSceneChange:
        // Invalidate cached controller; the new scene may differ.
        m_cachedController = nullptr;
        break;
    case ItemSceneHasChanged:
        // Populate cache lazily on next interactionController() call.
        break;
    case ItemPositionChange: {
        if (auto* d = dynamic_cast<Input::IDraggable*>(this)) {
            Input::ClickContext ctx = buildMinimalContext();
            QPointF allowed = d->onDragUpdate(value.toPointF(), ctx);
            return allowed;
        }
        break;
    }
    case ItemPositionHasChanged:
        m_draggingSincePress = true;
        break;
    default:
        break;
    }
    return QGraphicsObject::itemChange(change, value);
}

void GraphicsObjectBase::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    m_draggingSincePress = false;
    QGraphicsObject::mousePressEvent(e);
}

void GraphicsObjectBase::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    QGraphicsObject::mouseMoveEvent(e);
}

void GraphicsObjectBase::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    QGraphicsObject::mouseDoubleClickEvent(e);
}

void GraphicsObjectBase::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    QGraphicsObject::contextMenuEvent(e);
}

void GraphicsObjectBase::hoverEnterEvent(QGraphicsSceneHoverEvent* e)
{
    QGraphicsObject::hoverEnterEvent(e);
}

void GraphicsObjectBase::hoverMoveEvent(QGraphicsSceneHoverEvent* e)
{
    QGraphicsObject::hoverMoveEvent(e);
}

void GraphicsObjectBase::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
    QGraphicsObject::hoverLeaveEvent(e);
}

void GraphicsObjectBase::mouseReleaseEvent(
    QGraphicsSceneMouseEvent* event)
{
    // Let Qt finish default handling (selection commit, drag end position, ...).
    QGraphicsObject::mouseReleaseEvent(event);

    if (m_draggingSincePress) {
        m_draggingSincePress = false;
        if (auto* d = dynamic_cast<Input::IDraggable*>(this)) {
            Input::ClickContext ctx = buildMinimalContext();
            qCDebug(lcGuiInputItem)
                << "GraphicsObjectBase: drag-end on"
                << metaObject()->className()
                << "finalPos =" << pos();
            d->onDragEnd(pos(), ctx);
        }
    }
}

Input::ClickContext GraphicsObjectBase::buildMinimalContext() const
{
    Input::ClickContext ctx;
    ctx.scenePos         = scenePos();
    ctx.screenPos        = QPoint();
    ctx.modifiers        = Qt::NoModifier;
    ctx.button           = Qt::NoButton;
    ctx.target           = const_cast<GraphicsObjectBase*>(this);
    ctx.itemsUnderCursor = {const_cast<GraphicsObjectBase*>(this)};
    ctx.controller       = nullptr;
    ctx.commandBus       = nullptr;
    ctx.currentMode      = nullptr;

    if (auto* gs = qobject_cast<GraphicsScene*>(scene())) {
        ctx.scene = gs;
        if (!gs->views().isEmpty()) {
            ctx.view = qobject_cast<GraphicsView*>(gs->views().first());
        }
        if (auto* ctrl = gs->inputController()) {
            ctx.controller  = ctrl;
            ctx.commandBus  = ctrl->commandBus();
            ctx.mainWindow  = ctrl->mainWindow();
            ctx.currentMode = ctrl->currentMode();
            ctx.document    = ctrl->document();
        }
    }
    return ctx;
}
