/**
 * @file TruckSimulationClient.h
 * @brief Client for truck network simulation
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#pragma once

#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Clients/TruckClient//MessageFormatter.h"
#include "Backend/Clients/TruckClient/AsyncTripManager.h"
#include "Backend/Clients/TruckClient/TripEndCallback.h"
#include "Backend/Clients/TruckClient/TruckState.h"
#include "Backend/Commons/ClientType.h"
#include "Backend/Commons/DirectedGraph.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include "ContainerManager.h"
#include "TransportationGraph.h"
#include <QMap>
#include <QReadWriteLock>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

/**
 * @class TruckSimulationClient
 * @brief Manages truck simulations with INTEGRATION
 *
 * Provides an interface for launching, controlling and
 * monitoring truck simulations, handling trip creation
 * and completion, and managing containers.
 *
 * Thread Safety Implementation:
 * - Uses ThreadSafetyUtils' ScopedReadLock for read-only operations
 * - Uses ThreadSafetyUtils' ScopedWriteLock for write operations
 * - Ensures all access to shared data structures is properly protected
 * - Prevents potential deadlocks through timeout mechanisms
 */
class TruckSimulationClient : public SimulationClientBase
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param exePath Path to simulation executable
     * @param parent Parent QObject
     * @param host Host for RabbitMQ connection
     * @param port Port for RabbitMQ connection
     */
    explicit TruckSimulationClient(
        const QString &exePath, QObject *parent = nullptr,
        const QString &host = "localhost", int port = 5672);

    /**
     * @brief Destructor
     */
    ~TruckSimulationClient() override;

    /**
     * @brief Initializes the client in its thread
     *
     * Configures thread-specific resources, such as
     * RabbitMQ heartbeat, after moving to its thread.
     * Called automatically via QThread::started signal.
     *
     * @param simulationTime Simulation time object
     * @param logger Optional logger for initialization
     * logging
     * @throws std::runtime_error If RabbitMQ handler setup
     * fails
     * @note Avoid manual invocation unless synchronized
     * @warning Call only once after thread start
     */
    void initializeClient(
        SimulationTime           *simulationTime,
        TerminalSimulationClient *terminalClient = nullptr,
        LoggerInterface *logger = nullptr) override;

    /**
     * @brief Defines a new simulator instance
     * @param networkName Network identifier
     * @param masterFilePath Path to master configuration
     * file
     * @param simTime Simulation duration in seconds
     * @param configUpdates Custom configuration parameters
     * @param argsUpdates Additional command-line arguments
     * @return True if simulator was defined successfully
     */
    bool defineSimulator(
        const QString &networkName,
        const QString &masterFilePath, double simTime,
        const QMap<QString, QVariant> &configUpdates = {},
        const QStringList             &argsUpdates   = {});

    /**
     * @brief Synchronously runs the simulator
     * @param networkNames List of network names to run
     * @return True if command was sent successfully
     */
    bool runSimulator(const QStringList &networkNames);

    /**
     * @brief Advance simulation by a specific time step
     * @param networkNames Networks to advance
     * @param deltaT Time step in seconds
     * @return True if step completed successfully
     */
    Q_INVOKABLE bool advanceByTimeStep(
        const QStringList& networkNames,
        double deltaT);

    /**
     * @brief Notify about terminal closure for rerouting
     * @param terminalId Closed terminal
     * @param alternativeId Alternative terminal
     */
    Q_INVOKABLE void notifyTerminalClosure(
        const QString& terminalId,
        const QString& alternativeId);

    /**
     * @brief Notify about terminal reopening
     * @param terminalId Reopened terminal
     */
    Q_INVOKABLE void notifyTerminalReopened(
        const QString& terminalId);

    /**
     * @brief Ends the simulator
     * @param networkNames List of network names to end
     * @return True if command was sent successfully
     */
    bool endSimulator(const QStringList &networkNames);

    /**
     * @brief Adds a trip synchronously
     * @param networkName Network identifier
     * @param originId Origin node identifier
     * @param destinationId Destination node identifier
     * @param containers List of containers to assign to
     * trip
     * @return Trip identifier or empty string if failed
     */
    QString addTrip(const QString &networkName,
                    const QString &originId,
                    const QString &destinationId,
                    const QList<ContainerCore::Container *>
                        &containers = {});

    /**
     * @brief Adds a trip asynchronously
     * @param networkName Network identifier
     * @param originId Origin node identifier
     * @param destinationId Destination node identifier
     * @param containers List of containers to assign to
     * trip
     * @return Future that completes when trip ends
     */
    QFuture<TripResult>
    addTripAsync(const QString &networkName,
                 const QString &originId,
                 const QString &destinationId,
                 const QList<ContainerCore::Container *>
                     &containers = {});

    /**
     * @brief Gets a truck state by ID
     * @param networkName Network identifier
     * @param tripId Trip identifier
     * @return Const pointer to truck state or nullptr if
     * not found
     */
    const TruckState *
    getTruckState(const QString &networkName,
                  const QString &tripId) const;

    /**
     * @brief Gets all truck states for a network
     * @param networkName Network identifier
     * @return List of const pointers to truck states
     */
    QList<const TruckState *> getAllNetworkTrucksStates(
        const QString &networkName) const;

    /**
     * @brief Gets simulation progress
     * @param networkName Network identifier
     * @return Progress percentage (0-100)
     */
    double
    getProgressPercentage(const QString &networkName) const;

    /**
     * @brief Gets simulation time
     * @param networkName Network identifier
     * @return time of the simulation
     */
    double
    getSimulationTime(const QString &networkName) const;

    /**
     * @brief Sets the transportation network graph
     * @param graph Pointer to transportation graph
     */
    void setNetworkGraph(
        const TransportationGraph<QString> *graph);

    /**
     * @brief Registers a callback for trip end events
     * @param callbackId Unique callback identifier
     * @param callback Function to call when trips end
     */
    void registerTripEndCallback(
        const QString                           &callbackId,
        std::function<void(const TripEndData &)> callback);

    /**
     * @brief Registers a callback for specific trip
     * @param tripId Trip identifier
     * @param callbackId Unique callback identifier
     * @param callback Function to call when this trip ends
     */
    void registerTripSpecificCallback(
        const QString &tripId, const QString &callbackId,
        std::function<void(const TripEndData &)> callback);

    /**
     * @brief Unregisters a trip end callback
     * @param callbackId Callback identifier to remove
     */
    void
    unregisterTripEndCallback(const QString &callbackId);

    /**
     * @brief Gets the container manager
     * @return Pointer to container manager
     */
    ContainerManager *getContainerManager() const;

protected:
    /**
     * @brief Processes incoming messages
     * @param message Message as JSON object
     */
    void
    processMessage(const QJsonObject &message) override;

private:
    /**
     * @brief Launches the simulator process
     * @param networkName Network identifier
     * @param masterFilePath Path to master configuration
     * file
     * @param simTime Simulation duration in seconds
     * @param args Command-line arguments
     * @return True if launch was successful
     */
    bool launchSimulator(const QString     &networkName,
                         const QString     &masterFilePath,
                         double             simTime,
                         const QStringList &args);

    /** Path to simulation executable */
    QString m_exePath;

    /** Map of network names to simulator processes */
    QMap<QString, QProcess *> m_processes;

    /** Map of network names to truck states */
    QMap<QString, QList<TruckState *>> m_truckStates;

    /** Map of network names to current simulation times */
    QMap<QString, double> m_simulationTimes;

    /** Map of network names to simulation horizons */
    QMap<QString, double> m_simulationHorizons;

    /** Map of network names to total simulation times */
    QMap<QString, double> m_totalSimTimes;

    /** Counter for generating trip IDs */
    int m_tripIdCounter = 10000;

    /** ID of last request message */
    int m_lastRequestId = -1;

    /** Counter for sent messages */
    int m_sentMsgCounter = 0;

    /** Read-write lock for thread synchronization 
     * 
     * Protects internal data structures from concurrent access.
     * Access to this lock should be managed using:
     * - Commons::ScopedReadLock for read-only operations
     * - Commons::ScopedWriteLock for write operations
     * 
     * Using read-write locks allows multiple concurrent readers
     * while ensuring exclusive access for writers, improving
     * performance for read-heavy workloads.
     */
    mutable QReadWriteLock m_dataMutex;

    /** Pointer to transportation network graph */
    const TransportationGraph<QString> *m_networkGraph =
        nullptr;

    /** Manager for trip end callbacks */
    TripEndCallbackManager *m_tripEndCallbackManager;

    /** Manager for asynchronous trip operations */
    AsyncTripManager *m_asyncTripManager;

    /** Manager for container tracking */
    ContainerManager *m_containerManager;

signals:
    /**
     * @brief Signal emitted when a trip ends
     * @param networkName Network identifier
     * @param tripId Trip identifier
     */
    void tripEnded(const QString &networkName,
                   const QString &tripId);

    /**
     * @brief Signal emitted with trip end data
     * @param tripData Trip end data
     */
    void tripEndedWithData(const TripEndData &tripData);
};

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
