#pragma once

/**
 * @file TerminalSimulationClient.h
 * @brief Defines TerminalSimulationClient for server
 * interaction
 * @author Ahmed Aredah
 * @date March 21, 2025
 *
 * This file declares the TerminalSimulationClient class,
 * which provides an interface to the TerminalSim server via
 * RabbitMQ for managing terminals, path segments, and
 * containers in the CargoNetSim simulation framework.
 *
 * @note Requires Qt framework and RabbitMQ connectivity.
 * @warning Manages pointers; caller must handle memory
 * cleanup.
 */

#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @class TerminalSimulationClient
 * @brief Client for interacting with TerminalSim server
 *
 * This class extends SimulationClientBase to manage
 * terminal simulation operations, including terminal
 * management, route addition, path finding, and container
 * handling via RabbitMQ.
 *
 * @note Uses raw pointers for flexibility; ownership
 * varies.
 * @warning Thread-safe via mutex; ensure proper pointer
 * management.
 *
 * Thread Safety Implementation:
 * - Uses ThreadSafetyUtils' ScopedReadLock for read-only
 * operations
 * - Uses ThreadSafetyUtils' ScopedWriteLock for write
 * operations
 * - Ensures all access to shared data structures is
 * properly protected
 * - Prevents potential deadlocks through timeout mechanisms
 */
class TerminalSimulationClient : public SimulationClientBase
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the client instance
     * @param parent Parent QObject, defaults to nullptr
     * @param host RabbitMQ host, defaults to "localhost"
     * @param port RabbitMQ port, defaults to 5672
     *
     * Initializes the client with connection parameters.
     */
    explicit TerminalSimulationClient(
        QObject       *parent = nullptr,
        const QString &host = "localhost", int port = 5672);

    /**
     * @brief Destroys the client, freeing owned resources
     *
     * Cleans up dynamically allocated memory for terminals,
     * paths, and dequeued containers.
     */
    ~TerminalSimulationClient() override;

    /**
     * @brief Resets the server to initial state
     * @return True if reset succeeds, else false
     *
     * Sends a reset command to clear server state.
     */
    Q_INVOKABLE bool resetServer();

    /**
     * @brief Sets cost function parameters for path finding
     * @param parameters Cost parameters as a QVariantMap
     * @return True if parameters were successfully set
     *
     * Updates the cost weights used by the graph for path
     * finding operations. Must include entries for default
     * and all transportation modes (Ship, Train, Truck)
     * with appropriate weight attributes (cost,
     * travellTime, distance, carbonEmissions, risk,
     * energyConsumption, terminal_delay, terminal_cost).
     */
    Q_INVOKABLE bool setCostFunctionParameters(
        const QVariantMap &parameters);

    /**
     * @brief Initializes client in its thread
     * @param simulationTime Simulation time object
     * @param logger Optional logger, defaults to nullptr
     * @throws std::runtime_error If initialization fails
     *
     * Sets up thread-specific resources and RabbitMQ
     * heartbeat.
     */
    void initializeClient(
        SimulationTime           *simulationTime,
        TerminalSimulationClient *terminalClient = nullptr,
        LoggerInterface *logger = nullptr) override;

    // Terminal Management
    /**
     * @brief Adds a terminal to the server
     * @param terminal Pointer to Terminal object
     * @return True if addition succeeds
     *
     * Sends terminal data for server-side addition.
     */
    Q_INVOKABLE bool addTerminal(const Terminal *terminal);

    /**
     * @brief Add multiple terminals at once
     * @param terminals List of terminals to add
     * @return True if the operation was successful
     */
    Q_INVOKABLE bool
    addTerminals(const QList<Terminal *> &terminals);

    /**
     * @brief Adds an alias to a terminal
     * @param terminalId Terminal identifier
     * @param alias Alias to associate
     * @return True if alias is added
     *
     * Associates an additional ID with a terminal.
     */
    Q_INVOKABLE bool
    addTerminalAlias(const QString &terminalId,
                     const QString &alias);

    /**
     * @brief Retrieves aliases for a terminal
     * @param terminalId Terminal identifier
     * @return List of aliases as QStringList
     *
     * Fetches all aliases linked to a terminal.
     */
    Q_INVOKABLE QStringList
    getTerminalAliases(const QString &terminalId);

    /**
     * @brief Removes a terminal from the server
     * @param terminalId Terminal identifier
     * @return True if removal succeeds
     *
     * Deletes a terminal and its data from the server.
     */
    Q_INVOKABLE bool
    removeTerminal(const QString &terminalId);

    /**
     * @brief Gets the total terminal count
     * @return Number of terminals on the server
     *
     * Retrieves the current count of terminals.
     */
    Q_INVOKABLE int getTerminalCount();

    /**
     * @brief Gets status of a terminal
     * @param terminalId Terminal identifier
     * @return Borrowed Terminal pointer from the internal
     *         cache, or nullptr if not found.
     * @note The returned pointer is **owned by this client**
     *       (parented to it, destroyed on server reset or on
     *       client destruction). Callers must NOT delete it;
     *       doing so corrupts the cache and crashes any later
     *       reader (e.g. `Path::fromJson`) that still holds
     *       the address.
     *
     * Fetches and returns terminal status as an object.
     */
    Q_INVOKABLE Terminal *
    getTerminalStatus(const QString &terminalId);

    // Route Management
    /**
     * @brief Adds a route segment to the server
     * @param route Pointer to PathSegment object
     * @return True if addition succeeds
     *
     * Sends a route segment for server-side addition.
     */
    Q_INVOKABLE bool addRoute(const PathSegment *route);

    /**
     * @brief Add multiple routes at once
     * @param routes List of routes to add
     * @return True if the operation was successful
     */
    Q_INVOKABLE bool
    addRoutes(const QList<PathSegment *> &routes);

    /**
     * @brief Updates route weight attributes
     * @param start Starting terminal ID
     * @param end Ending terminal ID
     * @param mode Transportation mode
     * @param attributes New attributes as JSON
     * @return True if update succeeds
     *
     * Modifies weight attributes of an existing route.
     */
    Q_INVOKABLE bool
    changeRouteWeight(const QString &start,
                      const QString &end, int mode,
                      const QJsonObject &attributes);

    // Auto-connection
    /**
     * @brief Connects terminals by interface modes
     * @return True if connections succeed
     *
     * Links terminals based on interface compatibility.
     */
    Q_INVOKABLE bool connectTerminalsByInterfaceModes();

    /**
     * @brief Connects terminals in a region by mode
     * @param region Region name
     * @return True if connections succeed
     *
     * Links terminals within a region by mode.
     */
    Q_INVOKABLE bool
    connectTerminalsInRegionByMode(const QString &region);

    /**
     * @brief Connects regions by transportation mode
     * @param mode Transportation mode
     * @return True if connections succeed
     *
     * Establishes inter-region links by mode.
     */
    Q_INVOKABLE bool connectRegionsByMode(int mode);

    // Path Finding
    /**
     * @brief Finds the shortest path between terminals
     * @param start Starting terminal ID
     * @param end Ending terminal ID
     * @param mode Transportation mode
     * @return List of PathSegment pointers
     * @note Caller must delete each pointer
     *
     * Computes the shortest path using the given mode.
     */
    Q_INVOKABLE QList<PathSegment *>
                findShortestPath(const QString &start,
                                 const QString &end, int mode);

    /**
     * @brief Finds top N shortest paths
     * @param start Starting terminal ID
     * @param end Ending terminal ID
     * @param n Number of paths to return
     * @param mode Transportation mode
     * @param skipDelays Skip same mode delays, default true
     * @return List of Path pointers
     * @note Caller must delete each pointer
     *
     * Retrieves the top N shortest paths.
     */
    Q_INVOKABLE QList<Path *> findTopPaths(
        const QString &start, const QString &end, int n,
        TransportationTypes::TransportationMode mode,
        bool skipDelays = true);

    // Container Management
    /**
     * @brief Adds a container to a terminal
     * @param terminalId Terminal identifier
     * @param container Container pointer
     * @param addTime Addition time, default -1.0
     * @return True if addition succeeds
     *
     * Adds a single container to the specified terminal.
     */
    Q_INVOKABLE bool
    addContainer(const QString                  &terminalId,
                 const ContainerCore::Container *container,
                 double addTime = -1.0);

    /**
     * @brief Adds multiple containers to a terminal
     * @param terminalId Terminal identifier
     * @param containers QString of Json of the containers
     * @param addTime Addition time, default -1.0
     * @return True if addition succeeds
     *
     * Adds multiple containers to the terminal.
     */
    Q_INVOKABLE bool
    addContainers(const QString &terminalId,
                  QString &containers, double addTime,
                  const QString &arrivalMode = "");

    /**
     * @brief Adds multiple containers to a terminal
     * @param terminalId Terminal identifier
     * @param containers List of container pointers
     * @param addTime Addition time, default -1.0
     * @param arrivalMode Transportation mode string
     * @return True if addition succeeds
     *
     * Adds multiple containers to the terminal.
     */
    Q_INVOKABLE bool addContainers(
        const QString                     &terminalId,
        QList<ContainerCore::Container *> &containers,
        double                             addTime = -1.0,
        const QString                     &arrivalMode = "");

    /**
     * @brief Adds containers from JSON data
     * @param terminalId Terminal identifier
     * @param json JSON string of containers
     * @param addTime Addition time, default -1.0
     * @param arrivalMode Transportation mode string
     * @return True if addition succeeds
     *
     * Parses and adds containers from JSON.
     */
    Q_INVOKABLE bool
    addContainersFromJson(const QString &terminalId,
                          const QString &json,
                          double         addTime = -1.0,
                          const QString &arrivalMode = "");

    /**
     * @brief Gets containers by departing time
     * @param terminalId Terminal identifier
     * @param time Departing time threshold
     * @param condition Comparison operator
     * @return List of container pointers
     * @note Server-owned; do not delete
     *
     * Fetches containers based on departure time.
     */
    Q_INVOKABLE QList<ContainerCore::Container *>
    getContainersByDepartingTime(const QString &terminalId,
                                 double         time,
                                 const QString &condition);

    /**
     * @brief Gets containers by added time
     * @param terminalId Terminal identifier
     * @param time Added time threshold
     * @param condition Comparison operator
     * @return List of container pointers
     * @note Server-owned; do not delete
     *
     * Fetches containers based on addition time.
     */
    Q_INVOKABLE QList<ContainerCore::Container *>
    getContainersByAddedTime(const QString &terminalId,
                             double         time,
                             const QString &condition);

    /**
     * @brief Gets containers by next destination
     * @param terminalId Terminal identifier
     * @param destination Next destination ID
     * @return List of container pointers
     * @note Server-owned; do not delete
     *
     * Fetches containers headed to a destination.
     */
    Q_INVOKABLE QList<ContainerCore::Container *>
                getContainersByNextDestination(
                    const QString &terminalId,
                    const QString &destination);

    /**
     * @brief Dequeues containers by destination
     * @param terminalId Terminal identifier
     * @param destination Next destination ID
     * @return List of container pointers
     * @note Caller must delete each pointer
     *
     * Removes and returns containers for a destination.
     */
    Q_INVOKABLE QList<ContainerCore::Container *>
                dequeueContainersByNextDestination(
                    const QString &terminalId,
                    const QString &destination);

    /**
     * @brief Gets container count for a terminal
     * @param terminalId Terminal identifier
     * @return Number of containers
     *
     * Retrieves the current container count.
     */
    Q_INVOKABLE int
    getContainerCount(const QString &terminalId);

    /**
     * @brief Gets available capacity of a terminal
     * @param terminalId Terminal identifier
     * @return Available capacity as double
     *
     * Retrieves the remaining capacity.
     */
    Q_INVOKABLE double
    getAvailableCapacity(const QString &terminalId);

    /**
     * @brief Gets maximum capacity of a terminal
     * @param terminalId Terminal identifier
     * @return Maximum capacity as double
     *
     * Retrieves the total capacity.
     */
    Q_INVOKABLE double
    getMaxCapacity(const QString &terminalId);

    /**
     * @brief Clears containers from a terminal
     * @param terminalId Terminal identifier
     * @return True if clearing succeeds
     *
     * Removes all containers from the terminal.
     */
    Q_INVOKABLE bool
    clearTerminal(const QString &terminalId);

    // System Dynamics Management
    /**
     * @brief Update System Dynamics for all terminals
     * @param currentTime Current simulation time in seconds
     * @param deltaT Time step in seconds
     * @return True if update succeeded
     */
    Q_INVOKABLE bool updateAllTerminalsSystemDynamics(
        double currentTime,
        double deltaT);

    /**
     * @brief Update System Dynamics for a specific terminal
     * @param terminalId Terminal to update
     * @param currentTime Current simulation time in seconds
     * @param deltaT Time step in seconds
     * @return True if update succeeded
     */
    Q_INVOKABLE bool updateTerminalSystemDynamics(
        const QString& terminalId,
        double currentTime,
        double deltaT);

    /**
     * @brief Get System Dynamics state for a terminal
     * @param terminalId Terminal to query
     * @return JSON object with SD state
     */
    Q_INVOKABLE QJsonObject getTerminalSystemDynamicsState(
        const QString& terminalId);

    /**
     * @brief Get runtime-state snapshots for a batch of terminals.
     * @param terminalIds Terminal ids to query
     * @return One runtime snapshot object per requested terminal
     */
    Q_INVOKABLE QJsonArray getTerminalsRuntimeState(
        const QStringList &terminalIds);

    /**
     * @brief Get runtime handling projections for a batch of terminals.
     * @param terminalIds Terminal ids to query
     * @return One projection object per requested terminal
     */
    Q_INVOKABLE QJsonArray getTerminalsRuntimeProjections(
        const QStringList &terminalIds);

    /**
     * @brief Get terminal execution results filtered by execution id/path ids.
     * @param executionId Execution id to query
     * @param terminalIds Optional runtime terminal ids to limit the search
     * @param pathIdentities Optional prepared path identities to limit results
     * @return Matching terminal execution result objects
     */
    Q_INVOKABLE QJsonArray getTerminalExecutionResults(
        const QString     &executionId,
        const QStringList &terminalIds = {},
        const QStringList &pathIdentities = {});

    /**
     * @brief Clear terminal execution results for an execution id.
     * @param executionId Execution id to clear
     * @param terminalIds Optional runtime terminal ids to limit the clear
     * @return Number of terminal execution records cleared
     */
    Q_INVOKABLE int clearTerminalExecutionResults(
        const QString     &executionId,
        const QStringList &terminalIds = {});

    // Serialization and Diagnostics
    /**
     * @brief Serializes the server graph
     * @return Graph state as JSON object
     *
     * Retrieves the current graph state from the server.
     */
    Q_INVOKABLE QJsonObject serializeGraph();

    /**
     * @brief Deserializes graph to server
     * @param graphData Graph data as JSON object
     * @return True if deserialization succeeds
     *
     * Restores server state from graph data.
     */
    Q_INVOKABLE bool
    deserializeGraph(const QJsonObject &graphData);

    /**
     * @brief Pings the server for connectivity
     * @param echo Optional echo string, defaults to ""
     * @return Server response as JSON object
     *
     * Tests server responsiveness with an optional echo.
     */
    Q_INVOKABLE QJsonObject ping(const QString &echo = "");

protected:
    /**
     * @brief Populates terminal-specific caches from an
     *        incoming event.
     *
     * Dispatched by the base class `processMessage` before
     * waiters are woken, so state (e.g. m_topPaths) is
     * visible by the time `sendCommandAndWait` returns.
     */
    void onEventReceived(const QString     &normalizedEvent,
                         const QJsonObject &message) override;

private:
    static QString makeTopPathsCacheKey(
        const QString &start, const QString &end, int mode,
        int requestedTopN,
        bool skipSameModeTerminalDelaysAndCosts);

    /**
     * @brief Handles terminal added event
     * @param message Event data from server
     */
    void onTerminalAdded(const QJsonObject &message);

    /**
     * @brief Handles terminals added event
     * @param message Event data from server
     */
    void onTerminalsAdded(const QJsonObject &message);

    /**
     * @brief Handles route added event
     * @param message Event data from server
     */
    void onRouteAdded(const QJsonObject &message);

    /**
     * @brief Handles routes added event
     * @param message Event data from server
     */
    void onRoutesAdded(const QJsonObject &message);

    /**
     * @brief Handles path found event
     * @param message Event data from server
     */
    void onPathsFound(const QJsonObject &message);

    /**
     * @brief Handles containers added event
     * @param message Event data from server
     */
    void onContainersAdded(const QJsonObject &message);

    /**
     * @brief Handles server reset event
     * @param message Event data from server
     */
    void onServerReset(const QJsonObject &message);

    /**
     * @brief Handles error occurrence event
     * @param message Event data from server
     */
    void onErrorOccurred(const QJsonObject &message);

    /**
     * @brief Handles terminal removed event
     * @param message Event data from server
     */
    void onTerminalRemoved(const QJsonObject &message);

    /**
     * @brief Handles terminal count event
     * @param message Event data from server
     */
    void onTerminalCount(const QJsonObject &message);

    /**
     * @brief Handles containers fetched event
     * @param message Event data from server
     */
    void onContainersFetched(const QJsonObject &message);

    /**
     * @brief Handles capacity fetched event
     * @param message Event data from server
     */
    void onCapacityFetched(const QJsonObject &message);

    void onSystemDynamicsState(const QJsonObject &message);
    void onTerminalRuntimeState(const QJsonObject &message);
    void onTerminalRuntimeProjections(const QJsonObject &message);
    void onTerminalExecutionResults(const QJsonObject &message);
    void onTerminalExecutionResultsCleared(
        const QJsonObject &message);

    /**
     * @brief Read-write lock for thread-safe data access
     *
     * Protects internal data structures from concurrent
     * access. Access to this lock should be managed using:
     * - Commons::ScopedReadLock for read-only operations
     * - Commons::ScopedWriteLock for write operations
     *
     * Using read-write locks allows multiple concurrent
     * readers while ensuring exclusive access for writers,
     * improving performance for read-heavy workloads.
     */
    mutable QReadWriteLock m_dataMutex;

    /**
     * @brief Map of terminal IDs to Terminal pointers
     */
    QMap<QString, Terminal *> m_terminalStatus;

    /**
     * @brief Map of terminal aliasses
     */
    QMap<QString, QStringList> m_terminalAliases;

    /**
     * @brief Map of path keys to PathSegment lists
     */
    QMap<QString, QList<PathSegment *>> m_shortestPaths;

    /**
     * @brief Map of path keys to Path lists
     */
    QMap<QString, QList<Path *>> m_topPaths;

    /**
     * @brief Map of terminal IDs to container lists
     */
    QMap<QString, QList<ContainerCore::Container *>>
        m_containers;

    /**
     * @brief Map of terminal IDs to capacity values
     */
    QMap<QString, double> m_capacities;

    QMap<QString, QJsonObject> m_terminalSystemDynamicsStates;
    QMap<QString, QJsonObject> m_terminalRuntimeStates;
    QMap<QString, QJsonObject> m_terminalRuntimeProjections;
    QJsonArray                 m_lastTerminalExecutionResults;
    QJsonObject                m_lastTerminalExecutionResultsCleared;

    /**
     * @brief serialized graph data.
     */
    QJsonObject m_serializedGraph;

    /**
     * @brief ping Response
     */
    QJsonObject m_pingResponse;

    /**
     * @brief Current count of terminals
     */
    int m_terminalCount = 0;
};

} // namespace Backend
} // namespace CargoNetSim

/**
 * @brief Declares metatype for TerminalSimulationClient
 */
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TerminalSimulationClient)

/**
 * @brief Declares metatype for pointer to client
 */
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TerminalSimulationClient *)
