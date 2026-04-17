#include "GraphicsScene.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QMessageBox>
#include <QtGui/qevent.h>

#include "../Controllers/UtilityFunctions.h"
#include "../Controllers/SceneVisibilityController.h"
#include "../Controllers/ConnectionController.h"
#include "../Items/ConnectionLine.h"
#include "../Items/DistanceMeasurementTool.h"
#include "../Items/GlobalTerminalItem.h"
#include "../Items/MapPoint.h"
#include "../Items/TerminalItem.h"
#include "../MainWindow.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Controllers/BasicButtonController.h"
#include "GUI/Widgets/GraphicsView.h"

namespace CargoNetSim
{
namespace GUI
{

GraphicsScene::GraphicsScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_connectMode(false)
    , m_linkTerminalMode(false)
    , m_unlinkTerminalMode(false)
    , m_measureMode(false)
    , m_globalPositionMode(false)
    , m_pickDestinationMode(false)
    , m_connectFirstItem(QVariant())
    , m_measurementTool(nullptr)
{
}

void GraphicsScene::setIsInPickDestinationMode(bool enabled)
{
    m_pickDestinationMode = enabled;
    qCDebug(lcGuiScene)
        << "GraphicsScene::setIsInPickDestinationMode:"
        << enabled;
    if (!views().isEmpty())
    {
        if (enabled)
            views().first()->setCursor(Qt::CrossCursor);
        else
            views().first()->unsetCursor();
    }
}

void GraphicsScene::addItemWithId(GraphicsObjectBase *item,
                                  const QString      &id)
{
    qCDebug(lcGuiScene) << "GraphicsScene::addItemWithId:"
                        << "id=" << id
                        << "type=" << typeid(*item).name();
    // First add the item to the scene using the base class
    // method
    QGraphicsScene::addItem(item);

    // Take ownership
    item->setParent(this);

    // Get the class key and store in type map
    QString className = QString(typeid(*item).name());

    // Initialize the inner map if needed
    if (!itemsByType.contains(className))
    {
        itemsByType[className] =
            QMap<QString, QGraphicsItem *>();
    }

    // Add to type-specific map
    itemsByType[className][id] = item;
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
    try
    {
        if (!parent())
        {
            qCWarning(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                  << "no parent, ignoring";
            return;
        }

        MainWindow *mainWindowObj =
            dynamic_cast<MainWindow *>(parent());

        if (!mainWindowObj)
        {
            qCWarning(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                  << "parent is not MainWindow";
            return;
        }

        // Handle global position setting mode
        if (m_globalPositionMode)
        {
            qCDebug(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                << "globalPositionMode at"
                                << event->scenePos();
            QList<QGraphicsItem *> clickedItems =
                items(event->scenePos());
            bool terminalFound = false;

            for (QGraphicsItem *item : clickedItems)
            {
                GlobalTerminalItem *globalTerminal =
                    dynamic_cast<GlobalTerminalItem *>(
                        item);
                if (globalTerminal
                    && globalTerminal
                           ->getLinkedTerminalItem())
                {
                    if (views().isEmpty())
                    {
                        qCWarning(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                              << "no views in globalPositionMode";
                        return;
                    }

                    // Call the set global position
                    // method through the controller
                    BasicButtonController::
                        setTerminalGlobalPosition(
                            mainWindowObj,
                            globalTerminal
                                ->getLinkedTerminalItem());

                    terminalFound = true;
                    break;
                }
            }

            // Exit the mode if we successfully processed a
            // terminal
            if (terminalFound)
            {
                m_globalPositionMode = false;

                // Get main window through the view's parent
                // to uncheck the button
                if (!views().isEmpty())
                {
                    QObject *mainWindowObj =
                        views().first()->parent();
                    if (mainWindowObj)
                    {
                        QObject *button =
                            mainWindowObj
                                ->findChild<QObject *>(
                                    "set_global_position_"
                                    "button");
                        if (button)
                        {
                            button->setProperty("checked",
                                                false);
                        }
                    }
                }
                return;
            }
        }
        // Handle pick-destination mode
        else if (m_pickDestinationMode)
        {
            qCDebug(lcGuiScene)
                << "GraphicsScene::mousePressEvent:"
                << "pickDestinationMode at"
                << event->scenePos();

            // Search ALL items at the click position for
            // a TerminalItem, GlobalTerminalItem, or a
            // MapPoint with a linked terminal. itemAt()
            // only returns the topmost item which may be
            // a MapPoint overlapping the terminal icon.
            const auto allAtPos = items(event->scenePos());
            QString termId, termName;

            for (QGraphicsItem *candidate : allAtPos)
            {
                // Direct TerminalItem hit
                if (auto *terminal =
                        dynamic_cast<TerminalItem *>(
                            candidate))
                {
                    termId = terminal->getTerminalId();
                    termName =
                        terminal->getProperty("Name")
                            .toString();
                    qCDebug(lcGuiScene)
                        << "GraphicsScene: pick found"
                        << "TerminalItem" << termId;
                    break;
                }
                // Direct GlobalTerminalItem hit
                if (auto *global =
                        dynamic_cast<GlobalTerminalItem *>(
                            candidate))
                {
                    termId = global->getTerminalId();
                    if (global->getLinkedTerminalItem())
                        termName =
                            global->getLinkedTerminalItem()
                                ->getProperty("Name")
                                .toString();
                    else
                        termName = termId;
                    qCDebug(lcGuiScene)
                        << "GraphicsScene: pick found"
                        << "GlobalTerminalItem" << termId;
                    break;
                }
                // MapPoint with a linked terminal
                if (auto *mp =
                        dynamic_cast<MapPoint *>(
                            candidate))
                {
                    if (mp->getLinkedTerminal())
                    {
                        auto *linked =
                            mp->getLinkedTerminal();
                        termId = linked->getTerminalId();
                        termName =
                            linked->getProperty("Name")
                                .toString();
                        qCDebug(lcGuiScene)
                            << "GraphicsScene: pick found"
                            << "MapPoint->Terminal"
                            << termId;
                        break;
                    }
                }
            }

            if (!termId.isEmpty())
            {
                qCDebug(lcGuiScene)
                    << "GraphicsScene::mousePressEvent:"
                    << "destinationPicked" << termId
                    << termName;
                setIsInPickDestinationMode(false);
                emit destinationPicked(termId, termName);
            }
            else
            {
                qCDebug(lcGuiScene)
                    << "GraphicsScene::mousePressEvent:"
                    << "pickDestinationMode: no terminal"
                    << "found at click position,"
                    << "staying in pick mode";
            }
            return;
        }
        // Handle measurement mode
        else if (m_measureMode)
        {
            qCDebug(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                << "measureMode at"
                                << event->scenePos();
            QPointF scenePos = event->scenePos();

            if (!m_measurementTool)
            {
                // First click - create measurement tool and
                // set start point
                if (!views().isEmpty())
                {
                    m_measurementTool =
                        new DistanceMeasurementTool(
                            dynamic_cast<GraphicsView *>(
                                views().first()));
                    addItemWithId(m_measurementTool,
                                  m_measurementTool->sceneRegistryKey());
                    m_measurementTool->setStartPoint(
                        scenePos);

                    // Show status message if we can find
                    // the main window
                    if (!views().isEmpty())
                    {
                        QObject *mainWindowObj =
                            views().first()->parent();
                        if (mainWindowObj)
                        {
                            MainWindow *mainWin =
                                dynamic_cast<MainWindow *>(
                                    mainWindowObj);
                            if (mainWin)
                            {
                                mainWin
                                    ->showStatusBarMessage(
                                        "Click again to "
                                        "complete "
                                        "measurement",
                                        2000);
                            }
                        }
                    }
                }
            }
            else
            {
                // Second click - complete measurement and
                // reset for next measurement
                m_measurementTool->setEndPoint(scenePos);
                m_measurementTool = nullptr;
                m_measureMode     = false;

                // Get main window through the view's parent
                // to uncheck the button and show message
                if (!views().isEmpty())
                {

                    QObject *button =
                        mainWindowObj->findChild<QObject *>(
                            "measure_action");
                    if (button)
                    {
                        button->setProperty("checked",
                                            false);
                    }

                    QGraphicsView *view = views().first();
                    view->unsetCursor();

                    QObject *statusBar =
                        mainWindowObj->findChild<QObject *>(
                            "statusBar");
                    if (statusBar)
                    {
                        QMetaObject::invokeMethod(
                            statusBar, "showMessage",
                            Q_ARG(QString,
                                  "Measurement complete"),
                            Q_ARG(int, 2000));
                    }
                }
                return;
            }
        }
        // Handle connection mode
        else if (m_connectMode)
        {
            qCDebug(lcGuiScene) << "GraphicsScene::mousePressEvent:"
                                << "connectMode at"
                                << event->scenePos();
            // Get the current connection
            // type and region
            const Backend::TransportationTypes::TransportationMode
                currentConnectionType =
                    mainWindowObj->getConnectionType();

            QString currentRegion =
                CargoNetSim::CargoNetSimController::
                    getInstance()
                        .getRegionDataController()
                        ->getCurrentRegion();

            QList<QGraphicsItem *> clickedItems =
                items(event->scenePos());
            QVariant terminalVariant;

            // Find a terminal item among clicked items
            for (QGraphicsItem *item : clickedItems)
            {
                TerminalItem *terminal =
                    dynamic_cast<TerminalItem *>(item);
                if (terminal)
                {
                    terminalVariant =
                        QVariant::fromValue(terminal);
                    break;
                }

                GlobalTerminalItem *globalTerminal =
                    dynamic_cast<GlobalTerminalItem *>(
                        item);
                if (globalTerminal)
                {
                    terminalVariant =
                        QVariant::fromValue(globalTerminal);
                    break;
                }
            }

            if (!terminalVariant.isNull())
            {
                if (m_connectFirstItem.isNull())
                {
                    // First terminal selected
                    m_connectFirstItem = terminalVariant;

                    // Show status message

                    mainWindowObj->showStatusBarMessage(
                        QString("Selected first terminal. "
                                "Click another terminal to "
                                "create a %1 connection.")
                            .arg(Backend::TransportationTypes::
                                     toString(
                                         currentConnectionType)),
                        3000);
                }
                else
                {
                    // Compare against first item
                    bool isSameItem = false;

                    if (m_connectFirstItem
                            .canConvert<TerminalItem *>()
                        && terminalVariant.canConvert<
                            TerminalItem *>())
                    {
                        isSameItem =
                            m_connectFirstItem
                                .value<TerminalItem *>()
                            == terminalVariant
                                   .value<TerminalItem *>();
                    }
                    else if (m_connectFirstItem.canConvert<
                                 GlobalTerminalItem *>()
                             && terminalVariant.canConvert<
                                 GlobalTerminalItem *>())
                    {
                        isSameItem =
                            m_connectFirstItem.value<
                                GlobalTerminalItem *>()
                            == terminalVariant.value<
                                GlobalTerminalItem *>();
                    }

                    if (isSameItem)
                    {
                        m_connectFirstItem =
                            QVariant(); // Reset to null

                        // Show error message
                        if (!views().isEmpty())
                        {
                            mainWindowObj
                                ->showStatusBarMessage(
                                    "Cannot connect "
                                    "terminal to "
                                    "itself.",
                                    2000);
                        }
                        return;
                    }

                    // Create connection through utility
                    // function

                    // Get first terminal
                    QGraphicsItem *firstItem = nullptr;
                    if (m_connectFirstItem
                            .canConvert<TerminalItem *>())
                    {
                        firstItem =
                            m_connectFirstItem
                                .value<TerminalItem *>();
                    }
                    else if (m_connectFirstItem.canConvert<
                                 GlobalTerminalItem *>())
                    {
                        firstItem = m_connectFirstItem.value<
                            GlobalTerminalItem *>();
                    }

                    // Get second terminal
                    QGraphicsItem *secondItem = nullptr;
                    if (terminalVariant
                            .canConvert<TerminalItem *>())
                    {
                        secondItem =
                            terminalVariant
                                .value<TerminalItem *>();
                    }
                    else if (terminalVariant.canConvert<
                                 GlobalTerminalItem *>())
                    {
                        secondItem = terminalVariant.value<
                            GlobalTerminalItem *>();
                    }

                    // Call createConnectionLine utility
                    // function
                    ConnectionLine *connection =
                        mainWindowObj->connectionCtrl()
                            ->createConnectionLine(
                                firstItem,
                                secondItem,
                                currentConnectionType);

                    if (connection)
                    {
                        mainWindowObj->showStatusBarMessage(
                            "Connection created. "
                            "Click another "
                            "terminal to "
                            "continue connecting.",
                            2000);

                        // Update scene visibility
                        mainWindowObj->sceneVisibility()
                            ->updateSceneVisibility();

                        // Set the second terminal
                        // as the first for the next
                        // connection
                        m_connectFirstItem = terminalVariant;
                    }
                    else
                    {
                        // If connection failed,
                        // reset first item
                        m_connectFirstItem = QVariant();
                    }

                    return;
                }
            }
        }
        else
        {
            // Check if clicked on empty area
            QList<QGraphicsItem *> clickedItems =
                items(event->scenePos());
            if (clickedItems.isEmpty()
                && !views().isEmpty())
            {
                // Clear selection and hide properties
                // panel
                clearSelection();
                UtilitiesFunctions::hidePropertiesPanel(
                    mainWindowObj);
            }

            // Pass the event to the base class for normal
            // handling
            QGraphicsScene::mousePressEvent(event);
        }
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in "
                                 "GraphicsScene::mousePressEvent:"
                              << e.what();
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in "
                                 "GraphicsScene::mousePressEvent";
    }
}

void GraphicsScene::keyPressEvent(QKeyEvent *event)
{
    qCDebug(lcGuiScene) << "GraphicsScene::keyPressEvent:"
                        << "key=" << event->key();
    // For Delete key, pass it up to MainWindow
    if (event->key() == Qt::Key_Delete
        || event->key() == Qt::Key_Backspace)
    {
        event->ignore();
        return;
    }

    QGraphicsScene::keyPressEvent(event);
}

} // namespace GUI
} // namespace CargoNetSim
