#pragma once

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QProgressBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QtCore/qdatetime.h>

#include "Controllers/HeartbeatController.h"
#include "GUI/Widgets/ScrollableToolBar.h"
#include "GUI/Widgets/SpinnerWidget.h"
#include "Items/GlobalTerminalItem.h"
#include "Items/RegionCenterPoint.h"
#include "Items/TerminalItem.h"
#include "Widgets/CustomMainWindow.h"
#include "Widgets/NetworkManagerDialog.h"
#include "Widgets/RegionManagerWidget.h"

namespace CargoNetSim
{
namespace GUI
{

class GraphicsView;
class GraphicsScene;
class PropertiesPanel;
class SettingsWidget;
class ShortestPathsTable;
class SplashScreen;
class ToolbarController;
class BasicButtonController;
class NetworkController;
class ViewController;
class UtilitiesFunctions;
class PathFindingWorker;
class SimulationValidationWorker;
class TerminalSelectionDialog;

/**
 * @brief Main application window for CargoNetSim
 *
 * The MainWindow class is implemented as a singleton
 * and manages the entire application UI, including
 * views, scenes, docks, and toolbars.
 */
class MainWindow : public CustomMainWindow
{
    Q_OBJECT

    // Declare the ToolbarController and
    // BasicButtonController classes as a friend
    friend class ToolbarController;
    friend class BasicButtonController;
    friend class ViewController;
    friend class NetworkController;
    friend class UtilitiesFunctions;
    friend class PathFindingWorker;
    friend class SimulationValidationWorker;
    friend class TerminalSelectionDialog;

public:
    /**
     * @brief Gets the singleton instance of MainWindow
     * @return Pointer to the MainWindow instance
     */
    static MainWindow *getInstance();

    /**
     * @brief Destructor
     */
    virtual ~MainWindow();

    /**
     * @brief Gets the currently active view
     * @return Pointer to the current GraphicsView
     */
    GraphicsView *getCurrentView() const;

    /**
     * @brief Gets the currently active scene
     * @return Pointer to the current GraphicsScene
     */
    GraphicsScene *getCurrentScene() const;

    /**
     * @brief Checks if the global view is active
     * @return True if global view is active, false
     * otherwise
     */
    bool isGlobalViewActive() const;

    /**
     * @brief Checks if the region view is active
     * @return True if region view is active, false
     * otherwise
     */
    bool isRegionViewActive() const;

    /**
     * @brief Handles linking terminals to nodes
     * @param item The item being linked (terminal or node)
     */
    void handleTerminalNodeLinking(QGraphicsItem *item);

    /**
     * @brief Handles unlinking terminals from nodes
     * @param item The item being unlinked
     */
    void handleTerminalNodeUnlinking(QGraphicsItem *item);

    /**
     * @brief Shows or hides the shortest paths table
     * @param show True to show the table, false to hide it
     */
    void toggleShortestPathsTable(bool show = true);

    /**
     * @brief Updates coordinates of all region centers and
     * terminals
     */
    void updateAllCoordinates();

    void showStatusBarMessage(QString message,
                              int     timeout = 0);

    void showStatusBarError(QString message,
                            int     timeout = 0);

    void startStatusProgress();
    void stopStatusProgress();

    /**
     * @brief Gets the connection type
     * @return The current connection type
     */
    QString getConnectionType() const
    {
        return currentConnectionType_;
    }

public slots:
    /**
     * @brief Displays an error message
     * @param errorText The error message to display
     */
    void showError(const QString &errorText);

    /**
     * @brief Updates server heartbeat information
     * @param serverId The ID of the server
     * @param timestamp The timestamp of the heartbeat
     */
    void updateServerHeartbeat(const QString &serverId,
                               float          timestamp);

    /**
     * @brief Shows a backend message in the status bar
     * @param message The message to display
     * @param status The status type (info, error, success)
     * @param timeout The display timeout in milliseconds
     */
    void
    updateBackendMessage(const QString &message,
                         const QString &status  = "info",
                         int            timeout = 5000);

    /**
     * @brief Shuts down the application
     */
    void shutdown();

    /**
     * @brief Clears backend message display
     */
    void clearBackendMessage();

signals:
    /**
     * @brief Signal emitted when the active region changes
     * @param region The name of the new active region
     */
    void regionChanged(const QString &region);

protected:
    /**
     * @brief Handles window close events
     * @param event The close event
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief Handles key press events
     * @param event The key event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * @brief Handles window resize events
     * @param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    // Message queue system
    struct StatusMessage
    {
        QString   message;
        int       timeout;
        QDateTime timestamp;
        bool      isError;
    };
    QList<StatusMessage> messageQueue_;
    bool                 isProcessingMessageQueue_;

    // Progress bar for status messages
    SpinnerWidget *statusSpinner_;

    // New private method to process the queue
    void processMessageQueue();

    /**
     * @brief Private constructor for singleton pattern
     */
    MainWindow();

    /**
     * @brief Initializes the UI components
     */
    void initializeUI();

    /**
     * @brief Sets up the status bar
     */
    void setupStatusBar();

    void setupRegionMapScene();

    /**
     * @brief Sets up the global map scene
     */
    void setupGlobalMapScene();

    /**
     * @brief Sets up the terminal library dock
     */
    void setupTerminalLibrary();

    /**
     * @brief Sets up the region manager dock
     */
    void setupRegionManager();

    /**
     * @brief Sets up the logging tab
     */
    void setupLoggingTab();

    /**
     * @brief Sets up dock widgets
     */
    void setupDocks();

    /**
     * @brief Sets up toolbars
     */
    void setupToolbar();

    /**
     * @brief Handles tab change events
     * @param index The new tab index
     */
    void handleTabChange(int index);

    /**
     * @brief Toggles between pan modes
     */
    void togglePanMode();

    /**
     * @brief Sets the current connection type
     * @param connectionType The type of connection to set
     */
    void setConnectionType(const QString &connectionType);

    /**
     * @brief Assigns selected items to the current region
     */
    void assignSelectedToCurrentRegion();

    /**
     * @brief Starts processing message queues
     */
    void startQueueProcessing();

    /**
     * @brief Processes log messages from the queue
     */
    void processLogQueue();

    /**
     * @brief Processes progress updates from the queue
     */
    void processProgressQueue();

    /**
     * @brief Appends a log message to the appropriate
     * widget
     * @param message The message to append
     * @param widgetIndex The index of the widget to append
     * to
     * @param isError True if the message is an error
     */
    void appendLog(const QString &message, int widgetIndex,
                   bool isError);

    /**
     * @brief Updates the visibility of toolbar groups
     * @param group The group to update
     * @param buttons The buttons in the group
     */
    void
    updateGroupVisibility(QGroupBox              *group,
                          const QList<QWidget *> &buttons);

protected:
    // UI elements
    QTabWidget    *tabWidget_;
    GraphicsScene *regionScene_;
    GraphicsScene *globalMapScene_;
    GraphicsView  *regionView_;
    GraphicsView  *globalMapView_;

    QWidget              *loggingTab_;
    QDockWidget          *propertiesDock_;
    PropertiesPanel      *propertiesPanel_;
    QDockWidget          *settingsDock_;
    SettingsWidget       *settingsWidget_;
    QDockWidget          *shortestPathTableDock_;
    ShortestPathsTable   *shortestPathTable_;
    QDockWidget          *libraryDock_;
    QListWidget          *libraryList_;
    QDockWidget          *regionManagerDock_;
    RegionManagerWidget  *regionManager_;
    NetworkManagerDialog *networkManagerDock_;

    // Logging UI elements
    QList<QTextEdit *>    logTextWidgets_;
    QList<QProgressBar *> progressBars_;
    QStringList           clientNames_;
    QTimer               *logTimer_;
    QTimer               *progressTimer_;

    // Status bar elements
    QLabel *statusLabel_;
    QLabel *backendReportLabel_;
    QLabel *backendIcon_;

    // Key data
    QList<QAction *> logActions_;

    // Connection management
    QMenu      *connectionMenu_;
    QStringList connectionTypes_;
    QString     currentConnectionType_;
    TerminalItem *
        selectedTerminal_; // For linking terminals to nodes

    // State management
    QMap<QWidget *, QList<int>>     toolsButtonsVisibility_;
    QMap<int, QList<int>>           tabsVisibility_;
    QMap<QDockWidget *, QMap<QString, QVariant>>
        windowVisibility_;
    QList<QSize> savedSplitterSizes_;
    int          previousTabIndex_;
    bool         tableWasVisible_;

    // Controllers
    HeartbeatController *heartbeatController_;

    // Toolbar organization
    ScrollableToolBar *toolbar_;
    QGroupBox  *viewImportGroup_;
    QGroupBox  *projectGroup_;
    QGroupBox  *toolsGroup_;
    QGroupBox  *measurementsGroup_;
    QGroupBox  *regionGroup_;
    QGroupBox  *networkImportGroup_;
    QGroupBox  *navigationGroup_;
    QGroupBox  *windowsGroup_;
    QGroupBox  *logsGroup_;
    QGroupBox  *networkToolsGroup_;
    QGroupBox  *simulationToolsGroup_;
    QGroupBox  *transportationVehiclesGroup_;
    QGroupBox  *visibilityGroup_;
    QComboBox  *regionCombo_;

    QToolButton *findShortestPathButton_;
    QToolButton *validatePathsButton_;

    // Button groups
    QList<QWidget *>     viewImportButtons_;
    QList<QWidget *>     projectButtons_;
    QList<QWidget *>     toolsButtons_;
    QList<QWidget *>     measurementsButtons_;
    QList<QWidget *>     regionWidgets_;
    QList<QWidget *>     networkImportButtons_;
    QList<QWidget *>     navigationButtons_;
    QList<QWidget *>     windowsButtons_;
    QList<QWidget *>     logsButtons_;
    QList<QWidget *>     networkToolsButtons_;
    QList<QWidget *>     simulationToolsButtons_;
    QList<QWidget *>     transportationVehiclesButtons_;
    QList<QWidget *>     visibilityButtons_;

    QToolButton *panModeButton_;
    QToolButton *connectButton_;
    QToolButton *linkTerminalButton_;
    QToolButton *unlinkTerminalButton_;
    QToolButton *setGlobalPositionButton_;
    QToolButton *measureButton_;

    // Singleton instance
    static MainWindow *instance_;

    // Current project file path
    QString currentProjectPath_;
};

} // namespace GUI
} // namespace CargoNetSim
