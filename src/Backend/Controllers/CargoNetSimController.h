/**
 * @file CargoNetSimController.h
 * @brief Controls multi-modal simulation clients
 * @author [Your Name]
 * @date 2025-03-22
 */

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <memory>

#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/NetworkController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/SimulationTime.h"

namespace CargoNetSim
{

/**
 * @class CargoNetSimController
 * @brief Central backend controller. Owns scenario data, network
 *        catalogs, vehicle inventory, configuration, and the worker
 *        thread pool used by the simulation clients.
 *
 * @par Lifetime (Tier 1, Option E)
 * Exactly one instance per process, stack-allocated in main()
 * (both GUI and CLI). For GUI the declaration sits between the
 * QApplication stack variable and the MainWindow stack variable,
 * so C++ stack unwinding destroys MainWindow first, then the
 * controller, then QApplication - the order required for safe
 * GUI-before-controller teardown.
 *
 * For test binaries, QTEST_MAIN generates main() so the
 * controller is instead heap-allocated in initTestCase() and
 * parented to QCoreApplication::instance(). Qt reclaims it at
 * process exit; test binaries are short-lived so production-
 * style stack discipline is unnecessary.
 *
 * Callers look up the instance via getInstance() (reference,
 * asserts if uninitialized) or instance() (nullable pointer,
 * safe to call during startup and shutdown windows).
 *
 * @par Thread affinity
 * Construction and destruction occur on the main thread; both
 * are enforced at runtime by qFatal in release builds. Worker
 * threads are created and joined inside the controller's
 * lifetime, so cross-thread reads of instance() are safe without
 * synchronization (construction happens-before worker spawn,
 * destruction happens-after worker join).
 *
 * @par Invariants
 * - Exactly one instance exists at any time; double construction
 *   fires qFatal (release-safe).
 * - Construction and destruction are main-thread only; off-thread
 *   fires qFatal.
 * - instance() returns nullptr before construction and after
 *   destruction; getInstance() asserts.
 */
class CargoNetSimController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Reference access to the singleton. Asserts at runtime
     * (qFatal in release) if no instance has been constructed. Use
     * instance() for nullable access during startup or shutdown
     * windows.
     */
    static CargoNetSimController &getInstance();

    /**
     * @brief Non-owning lookup of the controller singleton.
     *
     * Returns nullptr if no instance has been constructed yet or if the
     * instance has already been destroyed. Prefer this over getInstance()
     * in code that may run during startup or shutdown windows.
     *
     * @return Pointer to the singleton, or nullptr.
     */
    static CargoNetSimController *instance();

    // Tier 1 ownership model: main() and test setup construct the
    // controller explicitly as a QObject parent-child of
    // QCoreApplication::instance(). Only one instance may exist at a
    // time - enforced by Q_ASSERT_X in the constructor body.
    explicit CargoNetSimController(
        Backend::LoggerInterface *logger = nullptr,
        QObject                  *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~CargoNetSimController();

    // Delete copy and move constructors and operators
    CargoNetSimController(const CargoNetSimController &) =
        delete;
    CargoNetSimController &
    operator=(const CargoNetSimController &) = delete;
    CargoNetSimController(CargoNetSimController &&) =
        delete;
    CargoNetSimController &
    operator=(CargoNetSimController &&) = delete;

    /**
     * @brief Initializes the controller and clients
     * @param truckExePath Path to truck simulation
     * executable
     * @return True if initialization was successful
     */
    bool initialize(const QString &truckExePath);

    /**
     * @brief Starts all simulation clients
     * @return True if all clients started successfully
     */
    bool startAll();

    /**
     * @brief Stops all simulation clients
     * @return True if all clients stopped successfully
     */
    bool stopAll();

    // ==========================================
    // Simulation Orchestration
    // ==========================================

    /**
     * @brief Run the simulation loop until completion
     * @return True if simulation completed successfully
     */
    Q_INVOKABLE bool runSimulation();

    /**
     * @brief Execute a single simulation time step
     * @return True if step completed, false if simulation ended
     */
    Q_INVOKABLE bool runSimulationStep();

    /**
     * @brief Pause the running simulation
     */
    Q_INVOKABLE void pauseSimulation();

    /**
     * @brief Resume a paused simulation
     */
    Q_INVOKABLE void resumeSimulation();

    /**
     * @brief Stop the simulation completely
     */
    Q_INVOKABLE void stopSimulation();

    /**
     * @brief Check if simulation is currently running
     * @return True if running
     */
    Q_INVOKABLE bool isSimulationRunning() const;

    /**
     * @brief Check if simulation is paused
     * @return True if paused
     */
    Q_INVOKABLE bool isSimulationPaused() const;

    /**
     * @brief Get current simulation time
     * @return Current time in seconds
     */
    Q_INVOKABLE double getCurrentSimulationTime() const;

    /**
     * @brief Set the simulation end time
     * @param endTime End time in seconds
     */
    Q_INVOKABLE void setSimulationEndTime(double endTime);

    // ==========================================
    // Dynamic Interventions
    // ==========================================

    /**
     * @brief Close a terminal and reroute traffic
     * @param terminalId Terminal to close
     * @param alternativeTerminalId Alternative terminal for rerouting
     * @return True if closure was successful
     */
    Q_INVOKABLE bool closeTerminal(const QString& terminalId,
                                   const QString& alternativeTerminalId);

    /**
     * @brief Reopen a previously closed terminal
     * @param terminalId Terminal to reopen
     * @return True if reopening was successful
     */
    Q_INVOKABLE bool reopenTerminal(const QString& terminalId);

    // Controller access methods

    /**
     * @brief Gets RegionDataController
     * @return
     */
    Backend::RegionDataController *
    getRegionDataController();

    /**
     * @brief Gets the vehicle controller
     * @return Pointer to vehicle controller
     */
    Backend::VehicleController *getVehicleController();

    /**
     *  @bried Gets the config controller
     *  @return Pointer to config controller
     */
    Backend::ConfigController *getConfigController();

    /**
     * @brief Gets the network controller
     * @return Pointer to network controller
     */
    Backend::NetworkController *
    getNetworkController() const;

    /**
     * @brief Gets the truck simulation manager
     * @return Pointer to truck simulation manager
     */
    Backend::TruckClient::TruckSimulationManager *
    getTruckManager() const;

    /**
     * @brief Gets the ship simulation client
     * @return Pointer to ship simulation client
     */
    Backend::ShipClient::ShipSimulationClient *
    getShipClient() const;

    /**
     * @brief Gets the train simulation client
     * @return Pointer to train simulation client
     */
    Backend::TrainClient::TrainSimulationClient *
    getTrainClient() const;

    /**
     * @brief Gets the terminal simulation client
     * @return Pointer to terminal simulation client
     */
    Backend::TerminalSimulationClient *
    getTerminalClient() const;

public slots:
    /**
     * @brief Gets terminal capacity
     * @param terminalId Terminal identifier
     * @return Available capacity
     */
    double getTerminalCapacity(const QString &terminalId);

    /**
     * @brief Gets terminal container count
     * @param terminalId Terminal identifier
     * @return Container count
     */
    int
    getTerminalContainerCount(const QString &terminalId);

    /**
     * @brief Adds containers to a terminal
     * @param terminalId Terminal identifier
     * @param containersJson Containers as JSON
     * @return True if successful
     */
    bool
    addContainersToTerminal(const QString &terminalId,
                            const QString &containersJson);

signals:
    /**
     * @brief Signal emitted when a client is initialized
     * @param clientType Type of client initialized
     */
    void clientInitialized(
        CargoNetSim::Backend::ClientType clientType);

    /**
     * @brief Signal emitted when all clients are
     * initialized
     */
    void allClientsInitialized();

    /**
     * @brief Signal emitted when a client is ready
     * @param clientType Type of client ready
     */
    void clientReady(
        CargoNetSim::Backend::ClientType clientType);

    /**
     * @brief Signal emitted when all clients are ready
     */
    void allClientsReady();

    ////////////// Terminal Client ////////////////////
    /**
     * @brief Signal to request terminal capacity
     * @param terminalId Terminal identifier
     * @param result Reference to store result
     */
    void requestTerminalCapacity(const QString &terminalId,
                                 double        &result);

    /**
     * @brief Signal to request container count
     * @param terminalId Terminal identifier
     * @param result Reference to store result
     */
    void requestContainerCount(const QString &terminalId,
                               int           &result);

    /**
     * @brief Signal to request container addition
     * @param terminalId Terminal identifier
     * @param containersJson Containers as JSON
     * @param result Reference to store result
     */
    void requestAddContainers(const QString &terminalId,
                              const QString &containersJson,
                              bool          &result);

    /**
     * @brief Emitted after each simulation step completes
     * @param currentTime Current simulation time after step
     * @param progress Overall progress percentage (0-100)
     */
    void simulationStepCompleted(double currentTime, double progress);

    /**
     * @brief Emitted when simulation completes
     */
    void simulationCompleted();

    /**
     * @brief Emitted when simulation is paused
     */
    void simulationPaused();

    /**
     * @brief Emitted when simulation is resumed
     */
    void simulationResumed();

    /**
     * @brief Emitted when a terminal is closed
     * @param terminalId The closed terminal
     * @param alternativeId The alternative terminal
     */
    void terminalClosed(const QString& terminalId,
                        const QString& alternativeId);

    /**
     * @brief Emitted when a terminal is reopened
     * @param terminalId The reopened terminal
     */
    void terminalReopened(const QString& terminalId);

private slots:
    /**
     * @brief Slot called when a thread has started
     */
    void onThreadStarted();

    /**
     * @brief Slot called when a thread has finished
     */
    void onThreadFinished();

private:
    // Tier 1 lifetime: single source of truth for the controller's
    // identity. Set by the constructor (via compare_exchange, which
    // also enforces the single-instance invariant atomically) and
    // cleared by the destructor with a release store. Atomic so that
    // cross-thread reads via instance() obey memory ordering even in
    // the edge case where ~CargoNetSimController's 3s thread-join
    // timeout is insufficient to stop a stuck worker: the release
    // store synchronizes with acquire loads in worker threads.
    static std::atomic<CargoNetSimController *> s_instance;

private:
    /**
     * @brief Creates and initializes the truck client
     * @param exePath Path to truck simulation executable
     * @return True if initialization was successful
     */
    bool initializeTruckClient(const QString &exePath);

    /**
     * @brief Creates and initializes the ship client
     * @return True if initialization was successful
     */
    bool initializeShipClient();

    /**
     * @brief Creates and initializes the train client
     * @return True if initialization was successful
     */
    bool initializeTrainClient();

    /**
     * @brief Creates and initializes the terminal client
     * @return True if initialization was successful
     */
    bool initializeTerminalClient();

    // SimulationTime
    Backend::SimulationTime *m_simulationTime;

    // Client threads
    QThread *m_truckThread;
    QThread *m_shipThread;
    QThread *m_trainThread;
    QThread *m_terminalThread;

    // Simulation clients
    Backend::TruckClient::TruckSimulationManager
        *m_truckManager;
    Backend::ShipClient::ShipSimulationClient *m_shipClient;
    Backend::TrainClient::TrainSimulationClient
                                      *m_trainClient;
    Backend::TerminalSimulationClient *m_terminalClient;

    // Controller instances
    Backend::RegionDataController *m_regionDataController;
    Backend::VehicleController    *m_vehicleController;
    Backend::NetworkController    *m_networkController;
    Backend::ConfigController     *m_configController;

    // Logger
    Backend::LoggerInterface *m_logger;

    // Track client initialization status
    QMap<Backend::ClientType, bool> m_clientInitialized;
    int                    m_initializedClientCount;
    int                    m_readyClientCount;

    // Simulation state
    bool m_simulationRunning = false;
    bool m_simulationPaused = false;
    double m_simulationEndTime = 0.0;

    // Closed terminals for rerouting
    QMap<QString, QString> m_closedTerminals;  // terminalId -> alternativeId

    // Private helper methods
    void advanceAllSimulators(double deltaT);
    void updateAllTerminalsSD(double currentTime, double deltaT);
    void processSimulatorEvents();
    bool checkSimulationComplete();
};

} // namespace CargoNetSim
