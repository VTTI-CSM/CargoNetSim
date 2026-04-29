/**
 * @file ShipSimulationClient.h
 * @brief Header for the ShipSimulationClient class
 *
 * This file defines the ShipSimulationClient class, which
 * manages interactions with the ship simulation server
 * within the CargoNetSim framework. It provides interfaces
 * for defining simulators, managing ships and containers,
 * and retrieving simulation states.
 *
 * Thread safety is ensured through careful use of
 * read-write locks for shared data protection. The class
 * leverages ThreadSafetyUtils to provide RAII-style
 * locking.
 *
 * @author Ahmed Aredah
 * @date March 19, 2025
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QSet>
#include <QString>
#include <containerLib/container.h>

#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Clients/ShipClient/ShipState.h"
#include "Backend/Clients/ShipClient/SimulationResults.h"
#include "Backend/Commons/ClientType.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include "Backend/Models/ShipSystem.h"

/**
 * @namespace CargoNetSim::Backend::ShipClient
 * @brief Namespace for ship simulation client components
 *
 * Contains classes and utilities for managing ship
 * simulation operations within the CargoNetSim backend.
 */
namespace CargoNetSim
{
namespace Backend
{

class SimulationTime;
class SimulatorHealthProbeTransport;

namespace ShipClient
{

/**
 * @class ShipSimulationClient
 * @brief Manages interactions with the ship simulation
 * server
 *
 * This class extends SimulationClientBase to provide
 * specialized functionality for ship simulation, including
 * simulator setup, ship and container management, and state
 * retrieval. It uses RabbitMQ for communication and ensures
 * thread-safe operations.
 *
 * Thread Safety:
 * - All operations that modify shared state (m_networkData,
 * m_shipState, etc.) are protected by appropriate read or
 * write locks
 * - Read operations use ScopedReadLock for concurrent
 * access
 * - Write operations use ScopedWriteLock for exclusive
 * access
 * - Event handlers that only log information do not use
 * locks
 * - Event handlers that modify state use appropriate
 * locking
 * - Messages from RabbitMQ are processed on the main Qt
 * event loop thread
 *
 * @ingroup ShipSimulation
 */
class ShipSimulationClient : public SimulationClientBase
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a ShipSimulationClient instance
     *
     * Initializes the client with a parent object and
     * RabbitMQ connection details. Defaults to localhost
     * and port 5672.
     *
     * @param parent Parent QObject, defaults to nullptr
     * @param host RabbitMQ server hostname, defaults to
     * "localhost"
     * @param port RabbitMQ server port, defaults to 5672
     */
    explicit ShipSimulationClient(
        QObject       *parent = nullptr,
        const QString &host = "localhost", int port = 5672);

    /**
     * @brief Destroys the ShipSimulationClient instance
     *
     * Cleans up resources, including dynamically allocated
     * objects and thread-safe data structures. This
     * operation acquires a write lock on m_dataAccessMutex
     * to safely clear all internal data structures.
     */
    ~ShipSimulationClient() override;

    /**
     * @brief Resets the ship simulation server
     *
     * Sends a reset command to the server, clearing all
     * current simulation data and state. This operation is
     * executed with command serialization for thread
     * safety.
     *
     * @return True if the reset command succeeds, false
     * otherwise
     */
    bool resetServer();

    /**
     * @brief Initializes the client within its thread
     *
     * Sets up thread-specific resources after the object is
     * moved to its thread. Automatically invoked via
     * QThread::started.
     *
     * @param simulationTime Pointer to the simulation time
     * @param logger Optional logger for initialization
     * logging
     * @throws std::runtime_error If RabbitMQ setup fails
     * @note Avoid manual calls unless synchronized
     * @warning Call only once after thread start
     */
    void initializeClient(
        SimulationTime           *simulationTime,
        TerminalSimulationClient *terminalClient = nullptr,
        LoggerInterface *logger = nullptr) override;

    /**
     * @brief Probe ship availability over the dedicated health transport
     *
     * Uses an isolated health command/response lane so liveness checks do not
     * queue behind simulation initialization or run commands.
     */
    bool probeCommandAvailability(int timeoutMs = 500) override;

    /**
     * @brief Defines a new ship simulator
     *
     * Configures a simulation network with specified ships
     * and parameters, sending the setup to the server. Uses
     * a write lock when updating internal data structures.
     *
     * Thread safety: Uses ScopedWriteLock when updating
     * shared state.
     *
     * @param networkName Unique name for the simulation
     * network
     * @param timeStep Time increment for simulation steps
     * @param ships List of Ship pointers to include in the
     * simulator
     * @param destinationTerminalIds Map of ship IDs to
     * terminal IDs
     * @param networkPath Path to network file, defaults to
     * "Default"
     * @return True if the simulator is defined successfully
     */
    bool
    defineSimulator(const QString       &networkName,
                    const double         timeStep,
                    const QList<Ship *> &ships,
                    const QMap<QString, QStringList>
                                  &destinationTerminalIds,
                    const QString &networkPath = "Default");

    /**
     * @brief Runs the simulator for a bounded interactive chunk
     *
     * Starts the simulation for the given networks or all if
     * "*" is specified, using an explicit chunk length. Uses a
     * read lock when accessing network data.
     *
     * Production code should prefer advanceByTimeStep(). This method
     * accepts only positive bounded chunks; unlimited execution is not
     * exposed by the CargoNetSim client API.
     *
     * Thread safety: Uses ScopedReadLock when reading network
     * keys.
     *
     * @param networkNames List of network names or "*" for all
     * @param byTimeSteps Explicit chunk length to request
     * @return True if the simulation starts successfully
     */
    bool runSimulator(const QStringList &networkNames,
                      double byTimeSteps);

    /**
     * @brief Advance simulation by a specific time step
     * @param networkNames Networks to advance ("*" for all)
     * @param deltaT Time step in seconds
     * @return True if step completed successfully
     *
     * Unlike runSimulator(), this waits for simulationAdvanced
     * event instead of allShipsReachedDestination.
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
     * @brief Ends the simulator for specified networks
     *
     * Terminates the simulation for given networks or all
     * if "*" is specified. Uses a read lock when accessing
     * network data.
     *
     * Thread safety: Uses ScopedReadLock when reading
     * network keys.
     *
     * @param networkNames List of network names or "*" for
     * all
     * @return True if the simulation ends successfully
     */
    bool endSimulator(const QStringList &networkNames);

    /**
     * @brief Adds ships to an existing simulator
     *
     * Incorporates additional ships into a running
     * simulation network, associating them with destination
     * terminals. Uses a write lock when updating ship data.
     *
     * Thread safety: Uses ScopedWriteLock when updating
     * ship data.
     *
     * @param networkName Target network name
     * @param ships List of Ship pointers to add
     * @param destinationTerminalIds Map of ship IDs to
     * terminal IDs
     * @return True if ships are added successfully
     */
    bool
    addShipsToSimulator(const QString       &networkName,
                        const QList<Ship *> &ships,
                        const QMap<QString, QStringList>
                            &destinationTerminalIds);

    /**
     * @brief Adds containers to a ship
     *
     * Assigns containers to a specified ship within a
     * network, sending the command to the server.
     *
     * Thread safety: Does not modify shared state directly,
     * uses executeSerializedCommand for thread safety.
     *
     * @param networkName Network containing the ship
     * @param shipId Unique identifier of the target ship
     * @param containers List of Container pointers to add
     * @return True if containers are added successfully
     */
    bool addContainersToShip(
        const QString &networkName, const QString &shipId,
        const QList<ContainerCore::Container *>
            &containers);

    /**
     * @brief Unloads containers from a ship at terminals
     *
     * Removes containers from a ship and assigns them to
     * specified terminals within a network.
     *
     * Thread safety: Does not modify shared state directly,
     * uses executeSerializedCommand for thread safety.
     *
     * @param networkName Network containing the ship
     * @param shipId Unique identifier of the target ship
     * @param terminalNames List of terminal names for
     * unloading
     * @return True if unloading succeeds
     */
    bool unloadContainersFromShipAtTerminals(
        const QString &networkName, const QString &shipId,
        const QStringList &terminalNames);

    /**
     * @brief Requests terminal nodes for a network
     *
     * Sends a command to retrieve the terminal nodes of a
     * specified simulation network.
     *
     * Thread safety: Does not modify shared state, only
     * sends a command.
     *
     * @param networkName Name of the network to query
     */
    void
    getNetworkTerminalNodes(const QString &networkName);

    /**
     * @brief Requests shortest path between nodes
     *
     * Sends a command to compute the shortest path between
     * two nodes in a specified network.
     *
     * Thread safety: Does not modify shared state, only
     * sends a command.
     *
     * @param networkName Network to query
     * @param startNode Starting node ID
     * @param endNode Ending node ID
     */
    void getShortestPath(const QString &networkName,
                         const QString &startNode,
                         const QString &endNode);

    /**
     * @brief Retrieves the state of a specific ship
     *
     * Returns the current state of a ship within a network.
     * Uses a read lock to safely access shared state.
     *
     * Thread safety: Uses ScopedReadLock for concurrent
     * read access.
     *
     * @param networkName Network containing the ship
     * @param shipId Unique identifier of the ship
     * @return Pointer to ShipState or nullptr if not found
     */
    const ShipState *
    getShipState(const QString &networkName,
                 const QString &shipId) const;

    /**
     * @brief Retrieves states of all ships in a network
     *
     * Returns a list of states for all ships in a specified
     * network. Uses a read lock to safely access shared
     * state.
     *
     * Thread safety: Uses ScopedReadLock for concurrent
     * read access.
     *
     * @param networkName Network to query
     * @return List of ShipState pointers, empty if none
     * found
     */
    QList<const ShipState *> getAllNetworkShipsStates(
        const QString &networkName) const;

    /**
     * @brief Retrieves states of all ships across all
     * networks
     *
     * Returns a map of network names to lists of ship
     * states. Uses a read lock to safely access shared
     * state.
     *
     * Thread safety: Uses ScopedReadLock for concurrent
     * read access.
     *
     * @return Map of network names to ShipState pointer
     * lists
     */
    QMap<QString, QList<const ShipState *>>
    getAllShipsStates() const;

protected:
    /**
     * @brief Processes messages from the server
     *
     * Handles incoming server messages, dispatching them to
     * appropriate event handlers. This method is called on
     * the Qt event thread via QueuedConnection from
     * RabbitMQHandler.
     *
     * Populates ship-specific caches from an incoming event.
     * Invoked by the base class `processMessage` before
     * waiters are woken (see SimulationClientBase).
     *
     * Thread safety: Event handlers that modify shared
     * state use appropriate locking mechanisms internally.
     */
    void onEventReceived(const QString     &normalizedEvent,
                         const QJsonObject &message) override;

signals:
    void segmentVehicleArrived(const QString &networkName,
                               const QString &vehicleId,
                               const QString &runtimeTerminalId,
                               double         eventTimeSeconds);

    void segmentUnloadSucceeded(const QString &networkName,
                                const QString &vehicleId,
                                const QString &terminalId,
                                double         eventTimeSeconds);

    void segmentUnloadFailed(const QString &networkName,
                             const QString &vehicleId,
                             const QString &terminalId,
                             const QString &message,
                             double         eventTimeSeconds);

    void terminalHandoffSucceeded(const QString &networkName,
                                  const QString &vehicleId,
                                  const QString &scenarioTerminalId,
                                  double         eventTimeSeconds);

    void terminalHandoffFailed(const QString &networkName,
                               const QString &vehicleId,
                               const QString &message,
                               double         eventTimeSeconds);

private:
    /**
     * @brief Internal method to unload containers
     *
     * Executes the unloading process and wait for a
     * response.
     *
     * Thread safety: Does not modify shared state directly,
     * only sends commands and processes responses.
     *
     * @param networkName Network name
     * @param shipId Ship ID
     * @param terminalNames Terminal names for unloading
     * @return True if the command is sent successfully
     */
    bool unloadContainersFromShipAtTerminalsPrivate(
        const QString &networkName, const QString &shipId,
        const QStringList &terminalNames,
        QString           *errorMessage = nullptr);

    /**
     * @brief Handles simulation network loaded event
     *
     * Processes the event when a simulation network is
     * loaded. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void
    onSimulationNetworkLoaded(const QJsonObject &message);

    /**
     * @brief Handles simulation created event
     *
     * Processes the event when a simulation is created.
     * Modifies the m_networkData structure.
     *
     * Thread safety: Uses ScopedWriteLock to protect shared
     * state.
     *
     * @param message Event data in JSON format
     */
    void onSimulationCreated(const QJsonObject &message);

    /**
     * @brief Handles simulation paused event
     *
     * Processes the event when a simulation is paused.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationPaused(const QJsonObject &message);

    /**
     * @brief Handles simulation resumed event
     *
     * Processes the event when a simulation is resumed.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationResumed(const QJsonObject &message);

    /**
     * @brief Handles simulation restarted event
     *
     * Processes the event when a simulation is restarted.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationRestarted(const QJsonObject &message);

    /**
     * @brief Handles simulation ended event
     *
     * Processes the event when a simulation ends.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationEnded(const QJsonObject &message);

    /**
     * @brief Handles simulation advanced event
     *
     * Processes the event when a simulation advances in
     * time. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationAdvanced(const QJsonObject &message);

    /**
     * @brief Handles simulation progress update event
     *
     * Processes the event when simulation progress updates.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void
    onSimulationProgressUpdate(const QJsonObject &message);

    /**
     * @brief Handles ship added to simulator event
     *
     * Processes the event when a ship is added to the
     * simulator. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onShipAddedToSimulator(const QJsonObject &message);

    /**
     * @brief Handles all ships reached destination event
     *
     * Processes the event when all ships reach their
     * destinations. Only performs logging, no shared state
     * is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onAllShipsReachedDestination(
        const QJsonObject &message);

    /**
     * @brief Handles ship reached destination event
     *
     * Processes the event when a ship reaches its
     * destination. Updates the m_shipState data structure
     * and completes final-destination unloading through the
     * planned scenario terminal when no explicit seaport
     * event is emitted.
     *
     * Thread safety: Uses ScopedWriteLock to protect shared
     * state. The lock is temporarily released while calling
     * unloadContainers to avoid potential deadlocks.
     *
     * @param message Event data in JSON format
     */
    void
    onShipReachedDestination(const QJsonObject &message);

    /**
     * @brief Handles ship reached seaport event
     *
     * Processes the event when a ship reaches a seaport.
     * Calls unloadContainersFromShipAtTerminalsPrivate but
     * doesn't directly modify shared state.
     *
     * Thread safety: No locks needed in this method.
     *
     * @param message Event data in JSON format
     */
    void onShipReachedSeaport(const QJsonObject &message);

    /**
     * @brief Handles containers unloaded event
     *
     * Processes the event when containers are unloaded from
     * a ship. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onContainersUnloaded(const QJsonObject &message);

    /**
     * @brief Handles simulation results available event
     *
     * Processes the event when simulation results are
     * available. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onSimulationResultsAvailable(
        const QJsonObject &message);

    /**
     * @brief Handles ship state available event
     *
     * Processes the event when a ship's state is available.
     * Updates the m_shipState data structure.
     *
     * Thread safety: Uses ScopedWriteLock to protect shared
     * state.
     *
     * @param message Event data in JSON format
     */
    void onShipStateAvailable(const QJsonObject &message);

    /**
     * @brief Handles simulator state available event
     *
     * Processes the event when the simulator state is
     * available. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void
    onSimulatorStateAvailable(const QJsonObject &message);

    /**
     * @brief Handles error occurred event
     *
     * Processes the event when an error occurs on the
     * server. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onErrorOccurred(const QJsonObject &message);

    /**
     * @brief Handles server reset event
     *
     * Processes the event when the server is reset.
     * Only performs logging, no shared state is modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     */
    void onServerReset();

    /**
     * @brief Handles containers added event
     *
     * Processes the event when containers are added to a
     * ship. Only performs logging, no shared state is
     * modified.
     *
     * Thread safety: No locks needed as this method only
     * logs.
     *
     * @param message Event data in JSON format
     */
    void onContainersAdded(const QJsonObject &message);

    /**
     * @var m_dataAccessMutex
     * @brief Read-write lock for thread-safe data access
     *
     * Ensures synchronized access to internal data
     * structures. Use ScopedReadLock for read operations
     * and ScopedWriteLock for write operations.
     */
    mutable QReadWriteLock m_dataAccessMutex;

    /**
     * @var m_networkData
     * @brief Stores simulation results by network
     *
     * Maps network names to lists of SimulationResults
     * pointers. Access to this structure is protected by
     * m_dataAccessMutex.
     */
    QMap<QString, QList<SimulationResults *>> m_networkData;

    /**
     * @var m_shipState
     * @brief Stores ship states by network
     *
     * Maps network names to lists of ShipState pointers.
     * Access to this structure is protected by
     * m_dataAccessMutex.
     */
    QMap<QString, QList<ShipState *>> m_shipState;

    /**
     * @var m_loadedShips
     * @brief Stores loaded ship objects
     *
     * Maps ship IDs to Ship pointers for the simulation.
     * Access to this structure is protected by
     * m_dataAccessMutex.
     */
    QMap<QString, Backend::Ship *> m_loadedShips;

    /**
     * @var m_shipsDestinationTerminals
     * @brief Maps ship IDs to destination terminals
     *
     * Associates each ship with its target terminal IDs.
     * Access to this structure is protected by
     * m_dataAccessMutex.
     */
    QMap<QString, QStringList> m_shipsDestinationTerminals;

    /**
     * @var m_finalDestinationHandledShips
     * @brief Tracks ships whose final destination event has
     *        already driven arrival/unload handling.
     *
     * Access to this structure is protected by
     * m_dataAccessMutex.
     */
    QSet<QString> m_finalDestinationHandledShips;

    SimulatorHealthProbeTransport *m_healthProbeTransport = nullptr;
};

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim

Q_DECLARE_METATYPE(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient
        *)
