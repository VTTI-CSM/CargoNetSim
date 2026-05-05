#pragma once

#include <QGraphicsItem>
#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QToolButton>

namespace CargoNetSim
{
namespace GUI
{

// Forward declarations
class MainWindow;
class GraphicsScene;
class TerminalItem;

/**
 * @class BasicButtonController
 * @brief Controller class for handling basic button
 * operations in the application
 *
 * This utility class provides static methods to handle
 * various button click events and actions in the main
 * application window. It manages button state toggling,
 * grid settings, connection modes, and other UI operations.
 */
class BasicButtonController
{
public:
    /**
     * @brief Resets all toggle buttons except the active
     * one
     * @param mainWindow Pointer to the main window
     * @param activeButton Pointer to the active button that
     *          should remain checked
     */
    static void
    resetOtherButtons(MainWindow  *mainWindow,
                      QToolButton *activeButton = nullptr);

    /**
     * @brief Toggles the grid visibility in both views
     * @param mainWindow Pointer to the main window
     * @param checked Whether the grid should be visible
     */
    static void toggleGrid(MainWindow *mainWindow,
                           bool        checked);

    /**
     * @brief Toggles the connect mode for connecting
     * terminals
     * @param mainWindow Pointer to the main window
     * @param checked Whether connect mode should be enabled
     */
    static void toggleConnectMode(MainWindow *mainWindow,
                                  bool        checked);

    /**
     * @brief Toggles the link terminal mode for linking
     * terminals to nodes
     * @param mainWindow Pointer to the main window
     * @param checked Whether link terminal mode should be
     * enabled
     */
    static void
    toggleLinkTerminalMode(MainWindow *mainWindow,
                           bool        checked);

    /**
     * @brief Toggles the unlink terminal mode for unlinking
     * terminals from nodes
     * @param mainWindow Pointer to the main window
     * @param checked Whether unlink terminal mode should be
     * enabled
     */
    static void
    toggleUnlinkTerminalMode(MainWindow *mainWindow,
                             bool        checked);

    /**
     * @brief Toggles the measurement mode for measuring
     * distances
     * @param mainWindow Pointer to the main window
     * @param checked Whether measurement mode should be
     * enabled
     */
    static void toggleMeasureMode(MainWindow *mainWindow,
                                  bool        checked);

    /**
     * @brief Clears all measurements from the scene
     * @param mainWindow Pointer to the main window
     */
    static void clearMeasurements(MainWindow *mainWindow);

    /**
     * @brief Changes the current region based on the combo
     * box selection
     * @param mainWindow Pointer to the main window
     */
    static void changeRegion(MainWindow    *mainWindow,
                             const QString &region);

    /**
     * @brief Exports the log data to a file
     * @param mainWindow Pointer to the main window
     */
    static void exportLog(MainWindow *mainWindow);

    /**
     * @brief Checks if all terminals in the network are
     * properly connected
     * @param mainWindow Pointer to the main window
     * @param scene The scene containing the terminals
     */
    static void checkNetwork(MainWindow    *mainWindow,
                             GraphicsScene *scene);

    /**
     * @brief Disconnects all terminals in the specified
     * region
     * @param mainWindow Pointer to the main window
     * @param scene The scene containing the terminals
     * @param region The region name or "*" for all regions
     */
    static void
    disconnectAllTerminals(MainWindow    *mainWindow,
                           GraphicsScene *scene,
                           const QString &region);

    /**
     * @brief Toggles the visibility of connection lines in
     * the current region
     * @param mainWindow Pointer to the main window
     * @param checked Whether connection lines should be
     * visible
     */
    static void
    toggleConnectionLines(MainWindow *mainWindow,
                          bool        checked);

    /**
     * @brief Toggles the visibility of terminals in the
     * current region
     * @param mainWindow Pointer to the main window
     * @param checked Whether terminals should be visible
     */
    static void toggleTerminals(MainWindow *mainWindow,
                                bool        checked);

    /**
     * @brief Creates a new project
     * @param mainWindow Pointer to the main window
     */
    static void newProject(MainWindow *mainWindow);

    /**
     * @brief Opens an existing scenario (YAML) and installs it on the
     *        MainWindow's runtime. Delegates YAML → ScenarioDocument to
     *        ScenarioPersistenceService and hands the document off to
     *        MainWindow::setRuntime, which wires observers (Task 21)
     *        that auto-populate the scene.
     * @param mainWindow Pointer to the main window
     */
    static void openScenario(MainWindow *mainWindow);

    /**
     * @brief Saves the currently-loaded scenario back to YAML. If no
     *        path has been chosen yet, prompts the user. Delegates
     *        ScenarioDocument → YAML to ScenarioPersistenceService.
     * @param mainWindow Pointer to the main window
     */
    static void saveScenario(MainWindow *mainWindow);

    /**
     * @brief Toggles the mode for setting global positions
     * of terminals
     * @param mainWindow Pointer to the main window
     * @param checked Whether the mode should be enabled
     */
    static void
    toggleSetGlobalPositionMode(MainWindow *mainWindow,
                                bool        checked);

    /**
     * @brief Toggles visibility of a dock widget
     * @param checked Whether the dock widget should be
     * visible
     * @param dockWidget The dock widget to toggle
     * @param button The button that controls the dock
     * widget
     * @param widgetName The name of the widget
     */
    static void toggleDockWidget(bool           checked,
                                 QDockWidget   *dockWidget,
                                 QToolButton   *button,
                                 const QString &widgetName);

    /**
     * @brief Shows the train manager dialog
     * @param mainWindow Pointer to the main window
     */
    static void showTrainManager(MainWindow *mainWindow);

    /**
     * @brief Shows the ship manager dialog
     * @param mainWindow Pointer to the main window
     */
    static void showShipManager(MainWindow *mainWindow);

    static void
    updateRegionComboBox(MainWindow *mainWindow);

    static void setupSignals(MainWindow *mainWindow);
};

} // namespace GUI
} // namespace CargoNetSim
