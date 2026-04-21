#pragma once

#include "GUI/Items/GraphicsObjectBase.h"
#include <QGraphicsScene>
#include <QPointF>
#include <QVariant>

namespace CargoNetSim
{
namespace GUI
{

namespace Input { class InteractionController; }

// Forward declarations
class TerminalItem;
class ConnectionLine;
class DistanceMeasurementTool;

/**
 * @brief Custom graphics scene for the CargoNetSim
 * application
 *
 * Post Plan-2 refactor: interaction-mode state lives in
 * Input::InteractionController, not on the scene. The scene
 * only translates raw Qt events into Input::InputEvent and
 * forwards them through the controller.
 */
class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit GraphicsScene(QObject *parent = nullptr);

    void addItemWithId(GraphicsObjectBase *item,
                       const QString      &id);

    /**
     * @brief Remove every tracked item: clears the `itemsByType` registry
     *        then delegates to QGraphicsScene::clear() which deletes the
     *        items themselves. Use this (not QGraphicsScene::clear()) on
     *        teardown, otherwise the type-indexed registry retains
     *        dangling pointers. Consumed by GUI::Scenario::SceneRepopulator.
     */
    void clearAll();

    // Get item by type and ID
    template <typename T> T *getItemById(const QString &id)
    {
        // Get the class key
        QString className = QString(typeid(T).name());

        // Check if this type and ID exist
        if (itemsByType.contains(className)
            && itemsByType[className].contains(id))
        {
            // Return the item if it's of the correct type
            return qgraphicsitem_cast<T *>(
                itemsByType[className][id]);
        }
        return nullptr;
    }

    // Get all items of a specific type
    template <typename T> QList<T *> getItemsByType()
    {
        QList<T *> result;
        QString    className = QString(typeid(T).name());

        // Check if this type exists
        if (itemsByType.contains(className))
        {
            // Iterate through all items of this type and
            // them to result
            for (auto item :
                 itemsByType[className].values())
            {
                // Check if the item is of the correct type
                T *typedItem =
                    qgraphicsitem_cast<T *>(item);
                if (typedItem)
                {
                    // Append to the result list
                    result.append(typedItem);
                }
            }
        }
        return result;
    }

    // Handle item removal
    template <typename T>
    bool removeItemWithId(const QString &id)
    {
        QString className = QString(typeid(T).name());

        // Check if this type and ID exist
        if (!itemsByType.contains(className)
            || !itemsByType[className].contains(id))
        {
            return false;
        }

        // Get the item
        QGraphicsItem *item = itemsByType[className][id];

        // Disconnect all signals from this item to prevent
        // callback errors
        if (QObject *obj = dynamic_cast<QObject *>(item))
        {
            QObject::disconnect(obj);
        }

        // Remove from type map
        itemsByType[className].remove(id);

        // Remove from scene
        QGraphicsScene::removeItem(item);

        // Delete the item
        delete item;
        item = nullptr;

        return true;
    }

    void setInputController(Input::InteractionController* ctrl)
    {
        m_inputController = ctrl;
    }

    Input::InteractionController* inputController() const
    {
        return m_inputController;
    }

protected:
    void mousePressEvent(
        QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(
        QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(
        QGraphicsSceneMouseEvent *event) override;

    void mouseDoubleClickEvent(
        QGraphicsSceneMouseEvent *event) override;

    void contextMenuEvent(
        QGraphicsSceneContextMenuEvent *event) override;

    void wheelEvent(
        QGraphicsSceneWheelEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    void dragEnterEvent(
        QGraphicsSceneDragDropEvent *event) override;

    void dragMoveEvent(
        QGraphicsSceneDragDropEvent *event) override;

    void dragLeaveEvent(
        QGraphicsSceneDragDropEvent *event) override;

    void dropEvent(
        QGraphicsSceneDragDropEvent *event) override;

private:
    // Nested map structure: outer key is class name, inner
    // key is item ID
    QMap<QString, QMap<QString, QGraphicsItem *>>
        itemsByType;

    Input::InteractionController *m_inputController = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim
