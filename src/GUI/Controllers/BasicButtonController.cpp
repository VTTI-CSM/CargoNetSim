#include "BasicButtonController.h"

#include <QComboBox>
#include <QCursor>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolButton>

#include "../Items/ConnectionLine.h"
#include "../Items/DistanceMeasurementTool.h"
#include "../Items/TerminalItem.h"
#include "../MainWindow.h"
#include "../Widgets/GraphicsScene.h"
#include "../Widgets/GraphicsView.h"
#include "../Widgets/SetCoordinatesDialog.h"

#include "../Controllers/NetworkController.h"
#include "../Controllers/UtilityFunctions.h"
#include "../Widgets/ShipManagerDialog.h"
#include "../Widgets/TrainManagerDialog.h"
#include "../Serializers/ProjectSerializer.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Controllers/SceneVisibilityController.h"
#include "GUI/Controllers/TerminalController.h"

namespace CargoNetSim
{
namespace GUI
{

void BasicButtonController::resetOtherButtons(
    MainWindow *mainWindow, QToolButton *activeButton)
{
    try
    {
        QList<QToolButton *> toggleButtons = {
            mainWindow->connectButton_,
            mainWindow->linkTerminalButton_,
            mainWindow->unlinkTerminalButton_,
            mainWindow->measureButton_};

        const int count = toggleButtons.size();
        qCDebug(lcGuiButton) << "BasicButtonController::resetOtherButtons: resetting" << count << "buttons";

        for (QToolButton *button : toggleButtons)
        {
            if (button != activeButton)
            {
                button->setChecked(false);
            }
        }

        // Reset associated modes in the scene
        mainWindow->regionScene_->setIsInConnectMode(false);
        mainWindow->regionScene_->setIsInLinkTerminalMode(
            false);
        mainWindow->regionScene_->setIsInUnlinkTerminalMode(
            false);
        mainWindow->regionScene_->setIsInMeasureMode(false);
        mainWindow->regionScene_->setConnectedFirstItem(
            QVariant());
        mainWindow->selectedTerminal_ = nullptr;
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in resetOtherButtons:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to reset buttons: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleGrid(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleGrid:"
                         << "checked=" << checked;
    try
    {
        // Update grid state for both views
        mainWindow->regionView_->setGridVisibility(checked);
        mainWindow->globalMapView_->setGridVisibility(
            checked);

        // Update button text
        QToolButton *sender = qobject_cast<QToolButton *>(
            mainWindow->sender());
        if (sender)
        {
            sender->setText(
                QString("%1\nGrid")
                    .arg(checked ? "Hide" : "Show"));
        }

        // Force update of both viewports
        mainWindow->regionView_->viewport()->update();
        mainWindow->globalMapView_->viewport()->update();

        mainWindow->statusBar()->showMessage(
            QString("Grid %1").arg(checked ? "enabled"
                                           : "disabled"),
            2000);
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleGrid:" << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to toggle grid: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleConnectMode(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleConnectMode:"
                         << "checked=" << checked;
    try
    {
        if (checked)
        {
            resetOtherButtons(mainWindow,
                              mainWindow->connectButton_);

            GraphicsScene *currentScene =
                mainWindow->tabWidget_->currentIndex() == 0
                    ? mainWindow->regionScene_
                    : mainWindow->globalMapScene_;

            currentScene->setIsInConnectMode(true);
            currentScene->setConnectedFirstItem(QVariant());
            mainWindow->statusBar()->showMessage(
                "Click on two terminals to connect them...",
                3000);
        }
        else
        {
            mainWindow->regionScene_->setIsInConnectMode(
                false);
            mainWindow->regionScene_->setConnectedFirstItem(
                QVariant());
            mainWindow->globalMapScene_->setIsInConnectMode(
                false);
            mainWindow->globalMapScene_
                ->setConnectedFirstItem(QVariant());
            mainWindow->connectButton_->setChecked(false);
            mainWindow->statusBar()->showMessage(
                "Connect mode disabled", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleConnectMode:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to toggle connect mode: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleLinkTerminalMode(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleLinkTerminalMode:"
                         << "checked=" << checked;
    try
    {
        if (checked)
        {
            resetOtherButtons(
                mainWindow,
                mainWindow->linkTerminalButton_);

            // Disable other modes when entering link mode
            mainWindow->regionScene_->setIsInConnectMode(
                false);
            mainWindow->regionScene_
                ->setIsInLinkTerminalMode(true);
            mainWindow->selectedTerminal_ = nullptr;
            mainWindow->statusBar()->showMessage(
                "Select a terminal, then select a node to "
                "link them...",
                3000);
        }
        else
        {
            mainWindow->regionScene_
                ->setIsInLinkTerminalMode(false);
            mainWindow->selectedTerminal_ = nullptr;
            mainWindow->linkTerminalButton_->setChecked(
                false);
            mainWindow->statusBar()->showMessage(
                "Link terminal mode disabled", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleLinkTerminalMode:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString(
                "Failed to toggle link terminal mode: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleUnlinkTerminalMode(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleUnlinkTerminalMode:"
                         << "checked=" << checked;
    try
    {
        if (checked)
        {
            resetOtherButtons(
                mainWindow,
                mainWindow->unlinkTerminalButton_);

            // Disable other modes when entering unlink mode
            mainWindow->regionScene_->setIsInConnectMode(
                false);
            mainWindow->regionScene_
                ->setIsInLinkTerminalMode(false);
            mainWindow->regionScene_
                ->setIsInUnlinkTerminalMode(true);
            mainWindow->selectedTerminal_ = nullptr;
            mainWindow->statusBar()->showMessage(
                "Select a terminal, then select a node to "
                "unlink them...",
                3000);
        }
        else
        {
            mainWindow->regionScene_
                ->setIsInUnlinkTerminalMode(false);
            mainWindow->selectedTerminal_ = nullptr;
            mainWindow->unlinkTerminalButton_->setChecked(
                false);
            mainWindow->statusBar()->showMessage(
                "Unlink terminal mode disabled", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleUnlinkTerminalMode:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString(
                "Failed to toggle unlink terminal mode: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleMeasureMode(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleMeasureMode:"
                         << "checked=" << checked;
    try
    {
        if (checked)
        {
            resetOtherButtons(mainWindow,
                              mainWindow->measureButton_);
        }

        GraphicsView *currentView =
            mainWindow->tabWidget_->currentIndex() == 0
                ? mainWindow->regionView_
                : mainWindow->globalMapView_;

        GraphicsScene *currentScene =
            dynamic_cast<GraphicsScene *>(
                currentView->scene());

        if (!currentScene)
        {
            qCWarning(lcGuiButton) << "BasicButtonController::toggleMeasureMode:"
                                   << "currentScene is null";
            return;
        }

        currentView->setMeasureMode(checked);
        currentScene->setIsInMeasureMode(checked);
        currentScene->setMeasurementTool(
            nullptr); // Reset measurement tool

        if (!checked)
        {
            if (currentView->getMeasurementTool())
            {
                if (currentView->getMeasurementTool()
                        ->scene())
                {
                    GraphicsScene *scene =
                        currentView->getScene();
                    if (scene)
                    {
                        scene->removeItemWithId<
                            DistanceMeasurementTool>(
                            currentView
                                ->getMeasurementTool()
                                ->sceneRegistryKey());
                    }
                }
                currentView->setMeasurementTool(nullptr);
            }
            currentView
                ->unsetCursor(); // Restore default cursor
            mainWindow->measureButton_->setChecked(false);
        }
        else
        {
            currentView->setCursor(
                QCursor(Qt::CrossCursor));
        }

        if (checked)
        {
            mainWindow->statusBar()->showMessage(
                "Click to set start point, click again to "
                "measure distance",
                3000);
        }
        else
        {
            mainWindow->statusBar()->showMessage(
                "Measurement mode disabled", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleMeasureMode:"
                    << e.what();

        // Reset measure mode to safe state
        mainWindow->measureButton_->setChecked(false);

        GraphicsView *currentView =
            mainWindow->tabWidget_->currentIndex() == 0
                ? mainWindow->regionView_
                : mainWindow->globalMapView_;

        if (currentView)
        {
            currentView->setMeasureMode(false);

            GraphicsScene *currentScene =
                dynamic_cast<GraphicsScene *>(
                    currentView->scene());

            if (currentScene)
            {
                currentScene->setIsInMeasureMode(false);
            }

            if (currentView->getMeasurementTool()
                && currentView->getMeasurementTool()
                       ->scene())
            {
                GraphicsScene *scene =
                    currentView->getScene();
                if (scene)
                {
                    scene->removeItemWithId<
                        DistanceMeasurementTool>(
                        currentView->getMeasurementTool()
                            ->sceneRegistryKey());
                }

                currentView->setMeasurementTool(nullptr);
            }

            currentView->unsetCursor();
        }

        QMessageBox::critical(
            mainWindow, "Error",
            QString("Error in measure mode: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::clearMeasurements(
    MainWindow *mainWindow)
{
    try
    {
        // Get current scene based on active tab
        GraphicsScene *currentScene =
            mainWindow->tabWidget_->currentIndex() == 0
                ? mainWindow->regionScene_
                : mainWindow->globalMapScene_;

        // Remove all measurement tools from the scene
        QList<GraphicsObjectBase *> itemsToRemove;

        auto measurementItems =
            currentScene
                ->getItemsByType<DistanceMeasurementTool>();
        for (DistanceMeasurementTool *convertedItem :
             measurementItems)
        {
            if (convertedItem)
            {
                itemsToRemove.append(convertedItem);
            }
        }

        // Remove items separately to avoid modifying
        // collection during iteration
        for (GraphicsObjectBase *item : itemsToRemove)
        {
            currentScene
                ->removeItemWithId<DistanceMeasurementTool>(
                    item->sceneRegistryKey());
        }

        mainWindow->statusBar()->showMessage(
            "All measurements cleared", 2000);
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in clearMeasurements:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to clear measurements: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::changeRegion(
    MainWindow *mainWindow, const QString &region)
{
    try
    {
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->setCurrentRegion(region);
        mainWindow->sceneVisibility()->updateSceneVisibility();
        emit mainWindow->regionChanged(region);
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in changeRegion:" << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to change region: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::exportLog(
    MainWindow *mainWindow)
{
    try
    {
        QString filePath = QFileDialog::getSaveFileName(
            mainWindow, "Save Log File", "",
            "Text Files (*.txt);;All Files (*.*)");

        if (!filePath.isEmpty())
        {
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly
                          | QIODevice::Text))
            {
                QTextStream stream(&file);

                // Write general log first
                int generalIndex =
                    mainWindow->clientNames_.size() - 1;
                stream << "--- "
                       << mainWindow
                              ->clientNames_[generalIndex]
                       << " ---\n";
                stream
                    << mainWindow
                           ->logTextWidgets_[generalIndex]
                           ->toPlainText();
                stream << "\n--------------------\n\n";

                // Write client logs
                for (int index = 0; index < generalIndex;
                     ++index)
                {
                    stream
                        << "--- "
                        << mainWindow->clientNames_[index]
                        << " ---\n";
                    stream << mainWindow
                                  ->logTextWidgets_[index]
                                  ->toPlainText();
                    stream << "\n--------------------\n\n";
                }

                file.close();
                mainWindow->statusBar()->showMessage(
                    QString("Log exported to %1")
                        .arg(filePath),
                    2000);
            }
            else
            {
                throw std::runtime_error(
                    QString("Could not open file for "
                            "writing: %1")
                        .arg(file.errorString())
                        .toStdString());
            }
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in exportLog:" << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to export log: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::checkNetwork(
    MainWindow *mainWindow, GraphicsScene *scene)
{
    qCDebug(lcGuiButton) << "BasicButtonController::checkNetwork: begin";
    try
    {
        QString currentRegion =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegion();
        QList<TerminalItem *> allRegionTerminals =
            UtilitiesFunctions::getTerminalItems(
                scene, currentRegion,
                "*", // terminal_type
                UtilitiesFunctions::ConnectionType::
                    Any, // any type
                UtilitiesFunctions::LinkType::Any // any
                                                  // type
            );

        QList<TerminalItem *> notConnectedTerminals =
            UtilitiesFunctions::getTerminalItems(
                scene, currentRegion,
                "*", // terminal_type
                UtilitiesFunctions::ConnectionType::
                    NotConnected, // not connected only type
                UtilitiesFunctions::LinkType::Any // any
                                                  // type
            );

        qCDebug(lcGuiButton) << "BasicButtonController::checkNetwork:"
                             << "total=" << allRegionTerminals.size()
                             << "notConnected=" << notConnectedTerminals.size();

        SceneVisibilityController::flashTerminalItems(
            notConnectedTerminals, true);

        if (!notConnectedTerminals.isEmpty())
        {
            mainWindow->showStatusBarMessage(
                "There are terminals that are not"
                "connected to any map point.",
                3000);
        }
        else
        {
            if (allRegionTerminals.isEmpty())
            {
                mainWindow->showStatusBarMessage(
                    "There are no terminals in the"
                    "current region.",
                    3000);
            }
            else
            {
                mainWindow->showStatusBarMessage(
                    "All terminals are connected", 2000);
            }
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in checkNetwork:" << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to check network: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::disconnectAllTerminals(
    MainWindow *mainWindow, GraphicsScene *scene,
    const QString &region)
{
    try
    {
        QList<GraphicsObjectBase *> itemsToRemove;

        auto itemsToCheck =
            scene->getItemsByType<ConnectionLine>();
        for (QGraphicsItem *item : itemsToCheck)
        {
            ConnectionLine *connection =
                dynamic_cast<ConnectionLine *>(item);
            if (connection
                && (region == "*"
                    || connection->getRegion() == region))
            {
                itemsToRemove.append(connection);
            }
        }

        // Remove items separately to avoid modifying
        // collection during iteration
        for (GraphicsObjectBase *item : itemsToRemove)
        {
            scene->removeItemWithId<ConnectionLine>(
                item->sceneRegistryKey());
        }

        if (mainWindow)
        {
            mainWindow->showStatusBarMessage(
                "All terminals disconnected", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in disconnectAllTerminals:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to disconnect terminals: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::toggleConnectionLines(
    MainWindow *mainWindow, bool checked)
{
    if (!mainWindow)
    {
        return;
    }

    try
    {
        // Update button text
        QToolButton *sender = qobject_cast<QToolButton *>(
            mainWindow->sender());
        if (sender)
        {
            sender->setText(
                QString("%1\nConnections")
                    .arg(checked ? "Hide" : "Show"));
        }

        // Get all connection lines in the region
        QList<ConnectionLine *> connectionLines =
            mainWindow->regionScene_
                ->getItemsByType<ConnectionLine>();

        QString currentRegion =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegion();

        // Update visibility of connection lines
        for (ConnectionLine *connection : connectionLines)
        {
            if (connection
                && connection->getRegion() == currentRegion)
            {
                connection->setVisible(checked);
            }
        }

        mainWindow->showStatusBarMessage(
            QString("Connection lines %1")
                .arg(checked ? "shown" : "hidden"),
            2000);
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleConnectionLines:"
                    << e.what();
        mainWindow->showStatusBarMessage(
            QString("Failed to toggle connection lines: %1")
                .arg(e.what()),
            3000);
    }
}

void BasicButtonController::toggleTerminals(
    MainWindow *mainWindow, bool checked)
{
    if (!mainWindow)
    {
        return;
    }

    try
    {
        // Update button text
        QToolButton *sender = qobject_cast<QToolButton *>(
            mainWindow->sender());
        if (sender)
        {
            sender->setText(
                QString("%1\nTerminals")
                    .arg(checked ? "Hide" : "Show"));
        }

        // Get terminals

        auto terminals =
            mainWindow->regionScene_
                ->getItemsByType<TerminalItem>();

        // Update visibility of terminals
        for (TerminalItem *terminal : terminals)
        {
            if (terminal
                && terminal->getRegion()
                       == CargoNetSim::CargoNetSimController::
                              getInstance()
                                  .getRegionDataController()
                                  ->getCurrentRegion())
            {
                terminal->setVisible(checked);
            }
        }

        mainWindow->statusBar()->showMessage(
            QString("Terminals %1")
                .arg(checked ? "shown" : "hidden"),
            2000);
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in toggleTerminals:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to toggle terminals: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::newProject(
    MainWindow *mainWindow)
{
    qCInfo(lcGuiButton) << "BasicButtonController::newProject: begin";
    try
    {
        QMessageBox::StandardButton reply =
            QMessageBox::question(
                mainWindow, "New Project",
                "Are you sure you want to start a new "
                "project? "
                "Any unsaved changes will be lost.",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            // Create an empty scenario with one default
            // region. ScenarioRuntime::load() calls clearAll()
            // + apply() on the backend, and setRuntime()
            // clears scenes + wires observers + backfills —
            // replacing all the manual cleanup that was here.
            auto doc = std::make_unique<
                Backend::Scenario::ScenarioDocument>();

            Backend::Scenario::RegionSpec defaultRegion;
            defaultRegion.name =
                QStringLiteral("Default Region");
            defaultRegion.color =
                QStringLiteral("#00FF00");
            defaultRegion.localOrigin    = {0.0, 0.0};
            defaultRegion.globalPosition = {0.0, 0.0};
            doc->addRegion(defaultRegion);

            auto runtime = std::make_unique<
                Backend::Scenario::ScenarioRuntime>(
                std::move(doc));

            if (!runtime->load())
            {
                qCCritical(lcGuiButton)
                    << "newProject: failed to initialize"
                       " empty scenario";
                QMessageBox::critical(
                    mainWindow, "New Project",
                    "Failed to initialize empty"
                    " scenario.");
                return;
            }

            mainWindow->setRuntime(std::move(runtime));
            mainWindow->currentProjectPath_.clear();

            qCInfo(lcGuiButton)
                << "newProject: empty scenario created";
            mainWindow->statusBar()->showMessage(
                "New project created", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in newProject:" << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to create new project: %1")
                .arg(e.what()));
    }
}

void BasicButtonController::openScenario(
    MainWindow *mainWindow)
{
    if (!mainWindow) return;
    qCInfo(lcGuiButton) << "BasicButtonController::openScenario: begin";

    const QString filePath = QFileDialog::getOpenFileName(
        mainWindow, "Open Scenario", "",
        "CargoNetSim Scenarios (*.yml *.yaml);;All Files (*.*)",
        nullptr, QFileDialog::DontUseNativeDialog);
    if (filePath.isEmpty()) return;

    qCInfo(lcGuiButton) << "BasicButtonController::openScenario:"
                        << "path=" << filePath;
    // Parse YAML → ScenarioDocument via the thin Task-18 wrapper; it
    // propagates the backend's human-readable error message so the
    // user sees what went wrong.
    QString err;
    auto    doc = ProjectSerializer::loadProject(filePath, &err);
    if (!doc)
    {
        qCWarning(lcGuiButton) << "BasicButtonController::openScenario:"
                               << "failed to load:" << err;
        QMessageBox::critical(
            mainWindow, "Open Scenario",
            QString("Failed to load scenario:\n%1").arg(err));
        return;
    }

    // Wrap the parsed document in a runtime and apply it to the backend
    // FIRST (populates CargoNetSimController's RegionDataController,
    // loads network files, etc.). setRuntime below then takes ownership
    // and triggers the scene backfill — which needs the backend ready
    // because SceneRepopulator's MapPoint rebuild reads RegionData.
    auto runtime =
        std::make_unique<Backend::Scenario::ScenarioRuntime>(
            std::move(doc));
    if (!runtime->load())
    {
        qCWarning(lcGuiButton) << "BasicButtonController::openScenario:"
                               << "runtime->load() failed";
        QMessageBox::critical(
            mainWindow, "Open Scenario",
            "Failed to apply scenario (validation error).");
        return;  // runtime drops out of scope, backend stays untouched beyond the partial apply
    }

    // Hand the loaded runtime to MainWindow. setRuntime clears the old
    // scenes, wires observers on the new doc, and performs a one-shot
    // SceneRepopulator::repopulate to backfill the scene from the
    // document state (see MainWindow.cpp comment).
    mainWindow->setRuntime(std::move(runtime));

    mainWindow->currentProjectPath_ = filePath;
    mainWindow->statusBar()->showMessage(
        QString("Scenario loaded: %1").arg(filePath), 3000);
}

void BasicButtonController::saveScenario(
    MainWindow *mainWindow)
{
    if (!mainWindow) return;
    qCInfo(lcGuiButton) << "BasicButtonController::saveScenario: begin";
    if (!mainWindow->runtime())
    {
        qCWarning(lcGuiButton) << "BasicButtonController::saveScenario:"
                               << "no scenario loaded";
        QMessageBox::warning(mainWindow, "Save Scenario",
                             "No scenario loaded.");
        return;
    }

    QString filePath = mainWindow->currentProjectPath_;
    if (filePath.isEmpty())
    {
        filePath = QFileDialog::getSaveFileName(
            mainWindow, "Save Scenario As", "",
            "CargoNetSim Scenarios (*.yml);;All Files (*.*)",
            nullptr, QFileDialog::DontUseNativeDialog);
        if (filePath.isEmpty()) return;
        if (!filePath.endsWith(".yml", Qt::CaseInsensitive)
            && !filePath.endsWith(".yaml", Qt::CaseInsensitive))
            filePath += ".yml";
        mainWindow->currentProjectPath_ = filePath;
    }

    QString err;
    if (!ProjectSerializer::saveProject(
            mainWindow->runtime()->document(), filePath, &err))
    {
        qCWarning(lcGuiButton) << "BasicButtonController::saveScenario:"
                               << "save failed:" << err;
        QMessageBox::critical(
            mainWindow, "Save Scenario",
            QString("Failed to save scenario:\n%1").arg(err));
        return;
    }
    mainWindow->statusBar()->showMessage(
        QString("Scenario saved: %1").arg(filePath), 3000);
}

void BasicButtonController::toggleSetGlobalPositionMode(
    MainWindow *mainWindow, bool checked)
{
    try
    {
        // Force reset the scene mode to match the button
        // state
        mainWindow->globalMapScene_
            ->setIsInGlobalPositionMode(checked);

        if (checked)
        {
            resetOtherButtons(
                mainWindow,
                mainWindow->setGlobalPositionButton_);
            mainWindow->statusBar()->showMessage(
                "Click on a terminal to set its global "
                "position...",
                3000);
        }
        else
        {
            mainWindow->setGlobalPositionButton_
                ->setChecked(false);
            mainWindow->statusBar()->showMessage(
                "Set global position mode disabled", 2000);
        }
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton)
            << "Error in toggleSetGlobalPositionMode:"
            << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString(
                "Failed to toggle global position mode: %1")
                .arg(e.what()));
    }
}

bool BasicButtonController::setTerminalGlobalPosition(
    MainWindow *mainWindow, TerminalItem *terminal)
{
    try
    {
        if (!mainWindow || !terminal)
        {
            return false;
        }

        // Get the terminal's current global position
        GlobalTerminalItem *globalItem =
            terminal->getGlobalTerminalItem();
        if (!globalItem)
        {
            mainWindow->showStatusBarMessage(
                "Terminal not found in global map", 2000);
            return false;
        }

        // Get the current position
        QPointF globalProjectedPos = globalItem->pos();

        QPointF globalGeoPos =
            mainWindow->globalMapView_->sceneToWGS84(
                globalProjectedPos);

        // Show the dialog
        SetCoordinatesDialog dialog(
            terminal->getProperties()["Name"].toString(),
            globalGeoPos, mainWindow);

        if (dialog.exec() == QDialog::Accepted)
        {
            // Get the new coordinates
            QPointF userGeoPoint = dialog.getCoordinates();

            bool result = mainWindow->terminalCtrl()
                ->updateTerminalPositionByGlobalPosition(
                    terminal, userGeoPoint);

            // Set the terminal position
            if (result)
            {
                mainWindow->statusBar()->showMessage(
                    "Terminal position updated", 2000);
                return true;
            }
            else
            {
                mainWindow->statusBar()->showMessage(
                    "Failed to update terminal position",
                    2000);
                return false;
            }
        }

        // Dialog was cancelled
        return false;
    }
    catch (const std::exception &e)
    {
        qCCritical(lcGuiButton) << "Error in setTerminalGlobalPosition:"
                    << e.what();
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to set terminal global "
                    "position: %1")
                .arg(e.what()));
        return false;
    }
}

void BasicButtonController::toggleDockWidget(
    bool checked, QDockWidget *dockWidget,
    QToolButton *button, const QString &widgetName)
{
    dockWidget->setVisible(checked);
    button->setText(QString("%1\n%2")
                        .arg(checked ? "Hide" : "Show")
                        .arg(widgetName));
}

void BasicButtonController::showTrainManager(
    MainWindow *mainWindow)
{
    TrainManagerDialog dialog(mainWindow);

    auto trains =
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController()
            ->getAllTrains();
    dialog.setTrains(trains);

    if (dialog.exec() == QDialog::Accepted)
    {
        // Store trains
        auto newTrains = dialog.getTrains();
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController()
            ->updateTrains(newTrains);
    }
}

void BasicButtonController::showShipManager(
    MainWindow *mainWindow)
{
    ShipManagerDialog dialog(mainWindow);

    auto ships =
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController()
            ->getAllShips();
    dialog.setShips(ships);
    dialog.updateTable();

    if (dialog.exec() == QDialog::Accepted)
    {
        // Store ships
        auto newShips = dialog.getShips();
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController()
            ->updateShips(newShips);
    }
}

void BasicButtonController::updateRegionComboBox(
    MainWindow *mainWindow)
{
    // Store current selection
    QString currentRegion =
        mainWindow->regionCombo_->currentText();

    // Clear and repopulate
    mainWindow->regionCombo_->clear();

    // Get all region names from RegionDataController
    QStringList regionNames =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getAllRegionNames();
    mainWindow->regionCombo_->addItems(regionNames);

    // Restore selection if it still exists, otherwise
    // select first item
    int index =
        mainWindow->regionCombo_->findText(currentRegion);
    if (index >= 0)
    {
        mainWindow->regionCombo_->setCurrentIndex(index);
    }
    else if (mainWindow->regionCombo_->count() > 0)
    {
        mainWindow->regionCombo_->setCurrentIndex(0);
        // Update current region in controller
        if (!mainWindow->regionCombo_->currentText()
                 .isEmpty())
        {
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->setCurrentRegion(
                        mainWindow->regionCombo_
                            ->currentText());
        }
    }
}

void BasicButtonController::setupSignals(
    MainWindow *mainWindow)
{
    QObject::connect(
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController(),
        &Backend::RegionDataController::regionAdded,
        mainWindow,
        [mainWindow](const QString &regionName) {
            updateRegionComboBox(mainWindow);
        });

    QObject::connect(
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController(),
        &Backend::RegionDataController::regionRenamed,
        mainWindow,
        [mainWindow](const QString &oldName,
                     const QString &newName) {
            updateRegionComboBox(mainWindow);
        });

    QObject::connect(
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController(),
        &Backend::RegionDataController::regionRemoved,
        mainWindow,
        [mainWindow](const QString &regionName) {
            updateRegionComboBox(mainWindow);
        });

    QObject::connect(
        mainWindow->regionCombo_,
        &QComboBox::currentTextChanged,
        [mainWindow](const QString &region) {
            BasicButtonController::changeRegion(mainWindow,
                                                region);
        });
}

} // namespace GUI
} // namespace CargoNetSim
