#include "GraphicsScene.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QKeyEvent>

#include "Backend/Commons/LogCategories.h"

#include "../Input/Handled.h"
#include "../Input/InputEvent.h"
#include "../Input/InteractionController.h"

namespace CargoNetSim
{
namespace GUI
{

GraphicsScene::GraphicsScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

GraphicsScene::~GraphicsScene()
{
    // Tear down tracked items while the registry is still alive. If we
    // let QGraphicsScene::~QGraphicsScene() delete items after the
    // derived members are already gone, destroyed() callbacks would
    // touch dead registry state.
    clearAll();
}

void GraphicsScene::addItemWithId(GraphicsObjectBase *item,
                                  const QString      &id)
{
    qCDebug(lcGuiScene) << "GraphicsScene::addItemWithId:"
                        << "id=" << id
                        << "type=" << typeid(*item).name();
    QGraphicsScene::addItem(item);

    const QString className = QString(typeid(*item).name());
    itemsByType[className][id] = item;

    // Self-healing registry invariant. Without this, any code path that
    // destroys the item without calling removeItemWithId (raw `delete`,
    // parent destruction, QGraphicsScene::clear(), re-entrant teardown)
    // leaves a dangling pointer in itemsByType. Later observers — which
    // lookup by (type, id) and then dereference — would crash on stale
    // pointers. Connecting destroyed() makes the registry self-pruning:
    // no matter who or how an item is deleted, its entry is erased
    // synchronously during ~QObject, before anyone can observe the
    // dangling state. className/id captured by value so the lambda
    // remains valid past `item`'s destruction. Qt::DirectConnection
    // (implicit — same thread) ensures synchronous execution.
    connect(item, &QObject::destroyed, this,
            [this, className, id](QObject *) {
                auto bucket = itemsByType.find(className);
                if (bucket == itemsByType.end())
                    return;
                bucket->remove(id);
                if (bucket->isEmpty())
                    itemsByType.erase(bucket);
            });
}

void GraphicsScene::clearAll()
{
    qCInfo(lcGuiScene) << "GraphicsScene::clearAll:"
                       << "typeCount=" << itemsByType.size()
                       << "sceneItemCount=" << items().size();
    // Drop registry references FIRST so the subsequent delete in
    // QGraphicsScene::clear() cannot leave us with dangling pointers.
    itemsByType.clear();
    QGraphicsScene::clear();
}

void GraphicsScene::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    if (m_inputController) {
        Input::PressEvent ie{
            event->button(), event->modifiers(),
            event->scenePos(), event->screenPos()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void GraphicsScene::keyPressEvent(QKeyEvent *event)
{
    if (m_inputController) {
        Input::KeyPressEvent ie{
            event->key(), event->modifiers(),
            event->text(), event->isAutoRepeat()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::keyPressEvent(event);
}

void GraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Move events are dispatched exclusively by GraphicsView::mouseMoveEvent
    // (the Adapter for the view-level input source). Dispatching here again
    // causes double-fire and, during scrollbar-driven panning, Qt synthesises
    // additional scene-level move events whose screenPos is the original
    // press location — they cancel the user's drag and freeze the pan.
    QGraphicsScene::mouseMoveEvent(event);
}

void GraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_inputController) {
        Input::ReleaseEvent ie{
            event->button(), event->modifiers(),
            event->scenePos(), event->screenPos()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void GraphicsScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_inputController) {
        Input::DoubleClickEvent ie{
            event->button(), event->modifiers(),
            event->scenePos(), event->screenPos()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}

void GraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    qCInfo(lcGuiScene)
        << "GraphicsScene::contextMenuEvent: scenePos=" << event->scenePos()
        << "hasController=" << (m_inputController != nullptr);
    if (m_inputController) {
        Input::ContextMenuRequest ie{
            event->modifiers(), event->scenePos(), event->screenPos()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            qCInfo(lcGuiScene) << "  dispatched Handled::Yes";
            event->accept();
            return;
        }
        qCInfo(lcGuiScene) << "  dispatch returned PassThrough";
    }
    QGraphicsScene::contextMenuEvent(event);
}

void GraphicsScene::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (m_inputController) {
        Input::WheelEvent ie{
            event->delta(), event->modifiers(),
            event->scenePos(), event->screenPos()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::wheelEvent(event);
}

void GraphicsScene::keyReleaseEvent(QKeyEvent *event)
{
    if (m_inputController) {
        Input::KeyReleaseEvent ie{ event->key(), event->modifiers() };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::keyReleaseEvent(event);
}

void GraphicsScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (m_inputController) {
        Input::DragEnterEvent ie{ event->mimeData(), event->scenePos() };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::dragEnterEvent(event);
}

void GraphicsScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (m_inputController) {
        Input::DragMoveEvent ie{ event->mimeData(), event->scenePos() };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::dragMoveEvent(event);
}

void GraphicsScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (m_inputController) {
        Input::DragLeaveEvent ie{};
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::dragLeaveEvent(event);
}

void GraphicsScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (m_inputController) {
        Input::DropEvent ie{ event->mimeData(), event->scenePos() };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsScene::dropEvent(event);
}

} // namespace GUI
} // namespace CargoNetSim
