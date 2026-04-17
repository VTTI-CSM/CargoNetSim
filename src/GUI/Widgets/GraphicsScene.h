#pragma once

#include "GUI/Items/GraphicsObjectBase.h"
#include <QGraphicsScene>
#include <QPointF>
#include <QVariant>

namespace CargoNetSim
{
namespace GUI
{

// Forward declarations
class TerminalItem;
class ConnectionLine;
class DistanceMeasurementTool;

/**
 * @brief Custom graphics scene for the CargoNetSim
 * application
 *
 * Extends QGraphicsScene to handle special interaction
 * modes like connection creation, terminal linking, and
 * measurement tools.
 */
class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a GraphicsScene
     * @param parent Parent QObject
     */
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

    bool isInConnectMode()
    {
        return m_connectMode;
    }
    bool isInLinkTerminalMode()
    {
        return m_linkTerminalMode;
    }
    bool isInUnlinkTerminalMode()
    {
        return m_unlinkTerminalMode;
    }
    bool isInMeasureMode()
    {
        return m_measureMode;
    }
    bool isInGlobalPositionMode()
    {
        return m_globalPositionMode;
    }
    QVariant getConnectedFirstItem()
    {
        return m_connectFirstItem;
    }
    DistanceMeasurementTool *getMeasurementTool()
    {
        return m_measurementTool;
    }

    void setIsInConnectMode(bool isInConnectMode)
    {
        m_connectMode = isInConnectMode;
    }
    void setIsInLinkTerminalMode(bool isInLinkTerminalMode)
    {
        m_linkTerminalMode = isInLinkTerminalMode;
    }
    void
    setIsInUnlinkTerminalMode(bool isInUnlinkTerminalMode)
    {
        m_unlinkTerminalMode = isInUnlinkTerminalMode;
    }
    void setIsInMeasureMode(bool isInMeasureMode)
    {
        m_measureMode = isInMeasureMode;
    }
    void
    setIsInGlobalPositionMode(bool isInGlobalPositionMode)
    {
        m_globalPositionMode = isInGlobalPositionMode;
    }
    bool isInPickDestinationMode() const
    {
        return m_pickDestinationMode;
    }
    void setIsInPickDestinationMode(bool enabled);
    void setConnectedFirstItem(QVariant connectedFirstItem)
    {
        m_connectFirstItem = connectedFirstItem;
    }
    void setMeasurementTool(
        DistanceMeasurementTool *measurementTool)
    {
        m_measurementTool = measurementTool;
    }

signals:
    /**
     * @brief Emitted when the user picks a destination
     *        terminal in pick-destination mode
     * @param terminalId   The terminal's unique ID
     * @param terminalName The terminal's display name
     */
    void destinationPicked(const QString &terminalId,
                           const QString &terminalName);

protected:
    /**
     * @brief Handles mouse press events in the scene
     * @param event The mouse event
     */
    void mousePressEvent(
        QGraphicsSceneMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

private:
    // Nested map structure: outer key is class name, inner
    // key is item ID
    QMap<QString, QMap<QString, QGraphicsItem *>>
        itemsByType;

    // Mode flags
    bool m_connectMode; ///< Flag indicating if connection
                        ///< creation mode is active
    bool m_linkTerminalMode; ///< Flag indicating if
                             ///< terminal-node linking mode
                             ///< is active
    bool m_unlinkTerminalMode; ///< Flag indicating if
                               ///< terminal-node unlinking
                               ///< mode is active
    bool m_measureMode; ///< Flag indicating if distance
                        ///< measurement mode is active
    bool m_globalPositionMode; ///< Flag indicating if
                               ///< setting global position
                               ///< mode is active
    bool m_pickDestinationMode; ///< Flag indicating if
                                ///< pick-destination mode is
                                ///< active

    // Objects used for connection and measurement modes
    QVariant
        m_connectFirstItem; ///< First terminal selected
                            ///< in connect mode (can be
                            ///< TerminalItem* or
                            ///< GlobalTerminalItem*)
    DistanceMeasurementTool
        *m_measurementTool; ///< Current measurement tool
                            ///< being used
};

} // namespace GUI
} // namespace CargoNetSim
