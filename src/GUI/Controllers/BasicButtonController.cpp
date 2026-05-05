#include "BasicButtonController.h"

#include <QComboBox>
#include <QCursor>
#include <QFileDialog>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolButton>

#include "../Items/ConnectionLine.h"
#include "../Items/DistanceMeasurementTool.h"
#include "../Items/TerminalItem.h"
#include "../MainWindow.h"
#include "../Widgets/ShortestPathTable.h"
#include "../Widgets/GraphicsScene.h"
#include "../Widgets/GraphicsView.h"
#include "../Widgets/SetCoordinatesDialog.h"

#include "../Controllers/ConnectionController.h"
#include "../Controllers/NetworkController.h"
#include "../Controllers/UtilityFunctions.h"
#include "../Widgets/ShipManagerDialog.h"
#include "../Widgets/TrainManagerDialog.h"
#include "../Input/InteractionController.h"
#include "../Input/Modes/ConnectMode.h"
#include "../Input/Modes/GlobalPositionMode.h"
#include "../Input/Modes/LinkTerminalMode.h"
#include "../Input/Modes/MeasureMode.h"
#include "../Input/Modes/NormalMode.h"
#include "../Input/Modes/UnlinkTerminalMode.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Application/ScenarioEditService.h"
#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Controllers/FleetController.h"
#include "GUI/Controllers/SceneVisibilityController.h"
#include "GUI/Controllers/TerminalController.h"

namespace CargoNetSim
{
namespace GUI
{

void BasicButtonController::resetOtherButtons(
    MainWindow *mainWindow, QToolButton *activeButton)
{
    if (!mainWindow) return;

    QList<QToolButton *> toggleButtons = {
        mainWindow->connectButton_,
        mainWindow->linkTerminalButton_,
        mainWindow->unlinkTerminalButton_,
        mainWindow->measureButton_};

    qCDebug(lcGuiButton) << "BasicButtonController::resetOtherButtons: resetting"
                         << toggleButtons.size() << "buttons";
    for (QToolButton *button : toggleButtons) {
        if (button && button != activeButton) {
            button->setChecked(false);
        }
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
    if (!mainWindow || !mainWindow->inputController()) {
        qCWarning(lcGuiButton) << "toggleConnectMode: null mainWindow or controller";
        return;
    }
    if (checked) {
        resetOtherButtons(mainWindow, mainWindow->connectButton_);
        mainWindow->inputController()->setMode<Input::ConnectMode>(
            mainWindow->getConnectionType());
    } else {
        mainWindow->inputController()->setMode<Input::NormalMode>();
    }
}

void BasicButtonController::toggleLinkTerminalMode(
    MainWindow *mainWindow, bool checked)
{
    if (!mainWindow || !mainWindow->regionInputController()) {
        qCWarning(lcGuiButton) << "toggleLinkTerminalMode: null mainWindow or region controller";
        return;
    }
    if (checked) {
        resetOtherButtons(mainWindow, mainWindow->linkTerminalButton_);
        mainWindow->regionInputController()->setMode<Input::LinkTerminalMode>();
    } else {
        mainWindow->regionInputController()->setMode<Input::NormalMode>();
    }
}

void BasicButtonController::toggleUnlinkTerminalMode(
    MainWindow *mainWindow, bool checked)
{
    if (!mainWindow || !mainWindow->regionInputController()) {
        qCWarning(lcGuiButton) << "toggleUnlinkTerminalMode: null mainWindow or region controller";
        return;
    }
    if (checked) {
        resetOtherButtons(mainWindow, mainWindow->unlinkTerminalButton_);
        mainWindow->regionInputController()->setMode<Input::UnlinkTerminalMode>();
    } else {
        mainWindow->regionInputController()->setMode<Input::NormalMode>();
    }
}

void BasicButtonController::toggleMeasureMode(
    MainWindow *mainWindow, bool checked)
{
    qCDebug(lcGuiButton) << "BasicButtonController::toggleMeasureMode:"
                         << "checked=" << checked;
    if (!mainWindow || !mainWindow->inputController()) {
        qCWarning(lcGuiButton) << "toggleMeasureMode: null mainWindow or controller";
        return;
    }
    if (checked) {
        resetOtherButtons(mainWindow, mainWindow->measureButton_);
        mainWindow->inputController()->setMode<Input::MeasureMode>();
    } else {
        mainWindow->inputController()->setMode<Input::NormalMode>();
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
        if (auto *networkView = mainWindow->networkViewService())
            networkView->setCurrentRegion(region);
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
            mainWindow && mainWindow->networkViewService()
                ? mainWindow->networkViewService()->currentRegionName()
                : QString();
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
        if (!mainWindow || !scene || !mainWindow->connectionCtrl())
        {
            qCWarning(lcGuiButton)
                << "disconnectAllTerminals: missing MainWindow, scene, or ConnectionController";
            return;
        }

        QList<ConnectionLine *> itemsToRemove;

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

        int removedCount = 0;
        for (ConnectionLine *item : itemsToRemove)
        {
            if (mainWindow->connectionCtrl()->removeConnectionLine(item))
                ++removedCount;
        }

        mainWindow->showStatusBarMessage(
            QString("Disconnected %1 connection(s).").arg(removedCount),
            2000);
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
            mainWindow->networkViewService()
                ? mainWindow->networkViewService()->currentRegionName()
                : QString();

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
                       == (mainWindow->networkViewService()
                               ? mainWindow->networkViewService()->currentRegionName()
                               : QString()))
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
            auto doc = std::make_unique<
                Backend::Scenario::ScenarioDocument>();

            Backend::Scenario::RegionSpec defaultRegion;
            defaultRegion.name =
                QStringLiteral("Default Region");
            defaultRegion.color =
                QStringLiteral("#00FF00");
            defaultRegion.localOrigin    = {0.0, 0.0};
            defaultRegion.globalPosition = {0.0, 0.0};
            if (!Backend::Application::ScenarioEditService::addRegion(
                    doc.get(), defaultRegion))
            {
                qCCritical(lcGuiButton)
                    << "newProject: failed to seed default region";
                QMessageBox::critical(
                    mainWindow, "New Project",
                    "Failed to initialize the default region.");
                return;
            }

            Backend::Application::ScenarioLoadService loadService;
            auto loadResult =
                loadService.loadFromDocument(std::move(doc));

            if (!loadResult.succeeded())
            {
                qCCritical(lcGuiButton)
                    << "newProject: failed to initialize"
                       " empty scenario:"
                    << loadResult.message;
                QMessageBox::critical(
                    mainWindow, "New Project",
                    QString("Failed to initialize empty"
                            " scenario.\n%1")
                        .arg(loadResult.message));
                return;
            }

            mainWindow->setRuntime(
                std::move(loadResult.runtime));
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
    QString err;
    Backend::Application::ScenarioPersistenceService persistenceService;
    auto doc = persistenceService.loadYaml(filePath, &err);
    if (!doc)
    {
        qCWarning(lcGuiButton)
            << "BasicButtonController::openScenario:"
            << "failed to load:" << err;
        QMessageBox::critical(
            mainWindow, "Open Scenario",
            QString("Failed to load scenario:\n%1")
                .arg(err));
        return;
    }

    Backend::Application::ScenarioLoadService loadService;
    auto loadResult =
        loadService.loadFromDocument(std::move(doc));
    if (!loadResult.succeeded())
    {
        qCWarning(lcGuiButton)
            << "BasicButtonController::openScenario:"
            << "failed to apply:" << loadResult.message;
        QMessageBox::critical(
            mainWindow, "Open Scenario",
            QString("Failed to apply scenario:\n%1")
                .arg(loadResult.message));
        return;
    }

    mainWindow->setRuntime(std::move(loadResult.runtime));

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

    auto &document = mainWindow->runtime()->document();
    const QList<QJsonObject> comparisonSnapshots =
        mainWindow->shortestPathTable_
            ? mainWindow->shortestPathTable_
                  ->buildComparisonSnapshots(document)
            : QList<QJsonObject>{};
    if (!Backend::Application::ScenarioEditService::
            updateComparisonSnapshots(&document, comparisonSnapshots))
    {
        qCWarning(lcGuiButton)
            << "BasicButtonController::saveScenario:"
            << "failed to update comparison snapshots";
    }

    QString err;
    Backend::Application::ScenarioPersistenceService persistenceService;
    if (!persistenceService.saveYaml(document, filePath, &err))
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
    if (!mainWindow || !mainWindow->globalInputController()) {
        qCWarning(lcGuiButton) << "toggleSetGlobalPositionMode: null mainWindow or controller";
        return;
    }
    if (checked) {
        resetOtherButtons(mainWindow, mainWindow->setGlobalPositionButton_);
        mainWindow->globalInputController()->setMode<Input::GlobalPositionMode>();
    } else {
        mainWindow->globalInputController()->setMode<Input::NormalMode>();
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
    if (!mainWindow) return;
    TrainManagerDialog dialog(mainWindow);

    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    auto trains = controller.getAllTrains();
    dialog.setTrains(trains);

    if (dialog.exec() == QDialog::Accepted)
    {
        // Store trains
        auto newTrains = dialog.getTrains();
        controller.updateTrains(newTrains);
        mainWindow->fleetCtrl()->appendTrainFiles(dialog.newlyLoadedFiles());
    }
}

void BasicButtonController::showShipManager(
    MainWindow *mainWindow)
{
    if (!mainWindow) return;
    ShipManagerDialog dialog(mainWindow);

    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    auto ships = controller.getAllShips();
    dialog.setShips(ships);
    dialog.updateTable();

    if (dialog.exec() == QDialog::Accepted)
    {
        // Store ships
        auto newShips = dialog.getShips();
        controller.updateShips(newShips);
        mainWindow->fleetCtrl()->appendShipFiles(dialog.newlyLoadedFiles());
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

    QStringList regionNames =
        mainWindow->networkViewService()
            ? mainWindow->networkViewService()->regionNames()
            : QStringList();
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
            if (auto *networkView =
                    mainWindow->networkViewService())
            {
                networkView->setCurrentRegion(
                    mainWindow->regionCombo_->currentText());
            }
        }
    }
}

void BasicButtonController::setupSignals(
    MainWindow *mainWindow)
{
    if (auto *networkView = mainWindow->networkViewService())
    {
        QObject::connect(
            networkView,
            &Backend::Application::NetworkViewService::regionAdded,
            mainWindow,
            [mainWindow](const QString &) {
                updateRegionComboBox(mainWindow);
            });

        QObject::connect(
            networkView,
            &Backend::Application::NetworkViewService::regionRenamed,
            mainWindow,
            [mainWindow](const QString &, const QString &) {
                updateRegionComboBox(mainWindow);
            });

        QObject::connect(
            networkView,
            &Backend::Application::NetworkViewService::regionRemoved,
            mainWindow,
            [mainWindow](const QString &) {
                updateRegionComboBox(mainWindow);
            });
    }

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
