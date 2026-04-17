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
#include "Controllers/StatusReporter.h"
#include "GUI/Widgets/ScrollableToolBar.h"
#include "GUI/Widgets/SpinnerWidget.h"
#include "Items/GlobalTerminalItem.h"
#include "Items/RegionCenterPoint.h"
#include "Items/TerminalItem.h"
#include "Backend/Commons/TransportationMode.h"
#include "Widgets/CustomMainWindow.h"
#include "Widgets/NetworkManagerDialog.h"
#include "Widgets/RegionManagerWidget.h"

#include <memory>

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
class ScenarioRuntime;
} // namespace Scenario
} // namespace Backend

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
class UtilitiesFunctions;
class TerminalSelectionDialog;
class SceneVisibilityController;
class TerminalController;
class NetworkDrawingController;
class ConnectionController;
class RegionController;

/**
 * @class MainWindow
 * @brief Application main window.
 *
 * @par Lifetime (Tier 1, Option E)
 * Stack-allocated in main() after the CargoNetSimController.
 * Stack unwinding destroys MainWindow before the controller,
 * giving GUI code access to a fully-alive controller during
 * widget destructors. Access the singleton via getInstance()
 * (reference, asserts if uninitialized) or instance() (nullable
 * pointer).
 *
 * @par Invariants
 * Only one instance may exist at a time (enforced by qFatal in
 * the constructor).
 */
class MainWindow : public CustomMainWindow, public StatusReporter
{
    Q_OBJECT

    // Declare the ToolbarController and
    // BasicButtonController classes as a friend
    friend class ToolbarController;
    friend class BasicButtonController;
    friend class NetworkController;
    friend class UtilitiesFunctions;
    friend class TerminalSelectionDialog;

public:
    /**
     * @brief Constructor. MainWindow is a Tier 1 lifetime
     * singleton (one instance per process) owned by main()'s
     * stack. Construction registers the instance in s_instance;
     * a second construction triggers qFatal.
     */
    MainWindow();

    /**
     * @brief Reference access to the MainWindow singleton.
     * Asserts (qFatal in release) if no MainWindow has been
     * constructed yet.
     */
    static MainWindow &getInstance();

    /**
     * @brief Nullable pointer lookup. Returns nullptr before
     * construction or after destruction.
     */
    static MainWindow *instance();

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
     * @brief Gets the region (WGS84-projecting) view.
     * @return Pointer to regionView_ (never null after construction).
     *
     * Public accessor exposed for GUI/Scenario factories that are not
     * friends of MainWindow but need to project lat/lon into scene
     * coordinates during repopulation.
     */
    GraphicsView *regionView() const { return regionView_; }

    /**
     * @brief Gets the global map view (Mercator-projecting).
     * @return Pointer to globalMapView_.
     */
    GraphicsView *globalMapView() const { return globalMapView_; }

    /**
     * @brief Gets the currently loaded ScenarioRuntime, or nullptr if
     *        no scenario has been opened yet (legacy GUI mode).
     *
     * Consumed by Tasks 14–16 (ViewController mutator delegations)
     * and Task 21 (document-signal observers).
     */
    Backend::Scenario::ScenarioRuntime *runtime() const
    {
        return m_runtime.get();
    }

    /**
     * @brief Install (or replace) the runtime. Takes ownership.
     *
     * Side effects:
     *   - Before swapping, clears both scenes via GraphicsScene::clearAll
     *     because scene items hold non-owning pointers into the previous
     *     runtime's document; destroying the old runtime would leave them
     *     dangling.
     *   - Passing a null runtime unloads the current scenario.
     *
     * Observer subscription to the document's signals is wired in
     * Task 21's `subscribeDocumentObservers()`.
     */
    void setRuntime(
        std::unique_ptr<Backend::Scenario::ScenarioRuntime> rt);

    /**
     * @brief Gets the region scene (QGraphicsScene for the active
     *        region-view tab).
     * @return Pointer to regionScene_.
     */
    GraphicsScene *regionScene() const { return regionScene_; }

    /**
     * @brief Gets the global map scene.
     * @return Pointer to globalMapScene_.
     */
    GraphicsScene *globalMapScene() const { return globalMapScene_; }

    /**
     * @brief Public accessor for the properties panel. Used by
     *        GUI/Scenario/ItemEventBinder to hook up coordinate
     *        refresh when a RegionCenterPoint is dragged without
     *        needing friendship.
     */
    PropertiesPanel *propertiesPanel() const { return propertiesPanel_; }

    // Sub-controller accessors
    SceneVisibilityController *sceneVisibility() const
    { return m_sceneVisibility; }
    TerminalController *terminalCtrl() const
    { return m_terminalCtrl; }
    NetworkDrawingController *networkDrawing() const
    { return m_networkDrawing; }
    ConnectionController *connectionCtrl() const
    { return m_connectionCtrl; }
    RegionController *regionCtrl() const
    { return m_regionCtrl; }

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

    /**
     * @brief Adds a background photo to the current view
     */
    void addBackgroundPhoto();

    /**
     * @brief Flashes the path lines for a selected path
     * @param pathId ID of the path to visualize
     */
    void flashPathLines(int pathId);

    void showStatusBarMessage(QString message,
                              int     timeout = 0);

    void showStatusBarError(QString message,
                            int     timeout = 0);

    void startStatusProgress();
    void stopStatusProgress();

    // StatusReporter implementation
    void showMessage(const QString &msg, int ms = 3000) override;
    void showError(const QString &msg, int ms = 5000) override;
    void startProgress() override;
    void stopProgress() override;
    void storeButtons() override;
    void disableButtons() override;
    void restoreButtons() override;

    /**
     * @brief Gets the connection type
     * @return The current connection type
     */
    Backend::TransportationTypes::TransportationMode
    getConnectionType() const
    {
        return currentConnectionType_;
    }

public slots:
    /**
     * @brief Displays an error dialog
     * @param errorText The error message to display
     */
    void showErrorDialog(const QString &errorText);

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
    /// Owned ScenarioRuntime (null in legacy GUI mode — no scenario
    /// opened). Lifetime tied to MainWindow. See Task 20 / Task 21.
    std::unique_ptr<Backend::Scenario::ScenarioRuntime> m_runtime;

    /// Subscribe this MainWindow to every ScenarioDocument signal on
    /// the currently installed runtime: document mutations drive scene
    /// rebuild / factory calls / item removal in one place. Called by
    /// setRuntime() after the runtime swap. Safe to call with a null
    /// m_runtime (no-ops).
    ///
    /// Qt's sender-auto-disconnect semantics: when the old runtime is
    /// destroyed in setRuntime, its ScenarioDocument is destroyed, and
    /// all connections that used it as sender tear down automatically.
    /// No bookkeeping needed on MainWindow's side.
    void subscribeDocumentObservers();

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
    void setConnectionType(
        Backend::TransportationTypes::TransportationMode
            connectionType);

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
    /// Available transport modes for user-created connection lines.
    /// Populated in the constructor with the three concrete modes.
    QList<Backend::TransportationTypes::TransportationMode>
        connectionTypes_;
    Backend::TransportationTypes::TransportationMode
        currentConnectionType_;
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
    HeartbeatController        *heartbeatController_;
    SceneVisibilityController  *m_sceneVisibility  = nullptr;
    TerminalController         *m_terminalCtrl     = nullptr;
    NetworkDrawingController   *m_networkDrawing   = nullptr;
    ConnectionController       *m_connectionCtrl   = nullptr;
    RegionController           *m_regionCtrl       = nullptr;

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

    // Singleton instance (Tier 1 lifetime: set by constructor,
    // cleared by destructor, owned by main()'s stack).
    static MainWindow *s_instance;

    // Current project file path
    QString currentProjectPath_;
};

} // namespace GUI
} // namespace CargoNetSim
