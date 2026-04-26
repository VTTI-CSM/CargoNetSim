/**
 * @file ShipSimulationClient.cpp
 * @brief Implementation of the ShipSimulationClient class
 *
 * This file contains the implementation of the
 * ShipSimulationClient class, providing functionality for
 * managing ship simulations, including setup, control, and
 * state retrieval within the CargoNetSim framework.
 *
 * Thread Safety Implementation Notes:
 * - Commands to the server are executed within
 * executeSerializedCommand to ensure thread-safe command
 * execution.
 * - Read operations on shared data use ScopedReadLock to
 * allow concurrent reads.
 * - Write operations on shared data use ScopedWriteLock for
 * exclusive access.
 * - Event handlers from RabbitMQ are processed on the Qt
 * event thread via QueuedConnection.
 * - Event handlers that only log information don't acquire
 * locks for better performance.
 * - Event handlers that modify shared state
 * (onSimulationCreated, onShipReachedDestination,
 *   onShipStateAvailable) use appropriate locking
 * mechanisms.
 * - In onShipReachedDestination, the lock is temporarily
 * released with unlock()/relock() while calling
 * unloadContainersFromShipAtTerminalsPrivate to avoid
 * deadlocks.
 *
 * @author Ahmed Aredah
 * @date March 19, 2025
 */

#include "ShipSimulationClient.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/LoggerInterface.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/SimulationTime.h"
// #include "TerminalGraphServer.h"
// #include "SimulatorTimeServer.h"
// #include "ProgressBarManager.h"
// #include "ApplicationLogger.h"

namespace CargoNetSim
{
namespace Backend
{
namespace ShipClient
{

namespace
{

QString previewContainers(const QJsonArray &containers)
{
    if (containers.isEmpty())
        return QStringLiteral("<empty>");
    return QString::fromUtf8(
        QJsonDocument(containers.first().toObject())
            .toJson(QJsonDocument::Compact));
}

} // namespace

/**
 * @brief Constructs a ShipSimulationClient instance
 *
 * Initializes the client with RabbitMQ connection
 * parameters and sets up base simulation client properties.
 *
 * @param parent Parent QObject, defaults to nullptr
 * @param host RabbitMQ hostname, defaults to "localhost"
 * @param port RabbitMQ port, defaults to 5672
 */
ShipSimulationClient::ShipSimulationClient(
    QObject *parent, const QString &host, int port)
    : SimulationClientBase(
          parent, host, port, "CargoNetSim.Exchange",
          "CargoNetSim.CommandQueue.ShipNetSim",
          "CargoNetSim.ResponseQueue.ShipNetSim",
          "CargoNetSim.Command.ShipNetSim",
          QStringList{"CargoNetSim.Response.ShipNetSim"},
          ClientType::ShipClient)
{
    qCInfo(lcClientShip) << "ShipSimulatorClient initialized";
}

/**
 * @brief Destroys the ShipSimulationClient instance
 *
 * Frees all dynamically allocated resources and logs
 * destruction.
 */
ShipSimulationClient::~ShipSimulationClient()
{
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_dataAccessMutex);
    for (auto &resultsList : m_networkData)
    {
        qDeleteAll(resultsList);
    }
    m_networkData.clear();
    for (auto &stateList : m_shipState)
    {
        qDeleteAll(stateList);
    }
    m_shipState.clear();
    qDeleteAll(m_loadedShips);
    m_loadedShips.clear();
    if (m_logger)
    {
        m_logger->log("ShipSimulationClient destroyed",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "ShipSimulatorClient destroyed";
    }
}

/**
 * @brief Resets the ship simulation server
 *
 * Clears all simulation data on the server and logs the
 * result.
 *
 * @return True if reset succeeds, false otherwise
 */
bool ShipSimulationClient::resetServer()
{
    return executeSerializedCommand([this]() {
        bool success = sendCommandAndWait(
            "resetServer", QJsonObject(), {"serverReset"});
        if (m_logger)
        {
            if (success)
            {
                m_logger->log(
                    "Server reset successful",
                    static_cast<int>(m_clientType));
            }
            else
            {
                m_logger->logError(
                    "Server reset failed",
                    static_cast<int>(m_clientType));
            }
        }
        return success;
    });
}

/**
 * @brief Initializes the client in its thread
 *
 * Configures RabbitMQ heartbeat and logs initialization
 * details.
 *
 * @param logger Optional logger for initialization logging
 * @throws std::runtime_error If RabbitMQ handler is not set
 */
void ShipSimulationClient::initializeClient(
    SimulationTime           *simulationTime,
    TerminalSimulationClient *terminalClient,
    LoggerInterface          *logger)
{
    SimulationClientBase::initializeClient(
        simulationTime, terminalClient, logger);
    if (m_rabbitMQHandler == nullptr)
    {
        if (m_logger)
        {
            m_logger->logError(
                "RabbitMQ handler not initialized",
                static_cast<int>(m_clientType));
        }
        throw std::runtime_error(
            "RabbitMQ handler not initialized");
    }
    m_rabbitMQHandler->setupHeartbeat(5);
    if (m_logger)
    {
        m_logger->log(
            "Initialized in thread: "
                + QString::number(
                    reinterpret_cast<quintptr>(
                        QThread::currentThreadId())),
            static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip)
            << "ShipSimulationClient initialized in thread:"
            << QThread::currentThreadId();
    }
}

/**
 * @brief Defines a new ship simulator
 *
 * Sets up a simulation with ships and parameters, storing
 * ship data.
 *
 * @param networkName Network name
 * @param timeStep Simulation time step
 * @param ships List of Ship pointers
 * @param destinationTerminalIds Ship ID to terminal IDs map
 * @param networkPath Network file path
 * @return True if successful
 */
bool ShipSimulationClient::defineSimulator(
    const QString &networkName, const double timeStep,
    const QList<Ship *> &ships,
    const QMap<QString, QStringList>
                  &destinationTerminalIds,
    const QString &networkPath)
{
    return executeSerializedCommand([&]() {
        try
        {
            QJsonArray shipsArray;
            for (const auto *ship : ships)
            {
                if (ship)
                {
                    shipsArray.append(ship->toJson());
                }
            }
            QJsonObject params;
            params["networkFilePath"] = networkPath;
            params["networkName"]     = networkName;
            params["timeStep"]        = timeStep;
            if (!ships.isEmpty())
            {
                params["ships"] = shipsArray;
            }
            bool success = sendCommandAndWait(
                "defineSimulator", params,
                {"simulationcreated"});
            if (success)
            {
                CargoNetSim::Backend::Commons::
                    ScopedWriteLock locker(
                        m_dataAccessMutex);
                for (auto *ship : ships)
                {
                    if (ship)
                    {
                        m_loadedShips[ship->getUserId()] =
                            ship;
                        auto terminals =
                            destinationTerminalIds.value(
                                ship->getUserId());
                        m_shipsDestinationTerminals
                            [ship->getUserId()] = terminals;
                    }
                }
            }
            return success;
        }
        catch (const std::exception &e)
        {
            if (m_logger)
            {
                m_logger->logError(
                    "Exception in defineSimulator: "
                        + QString(e.what()),
                    static_cast<int>(m_clientType));
            }
            else
            {
                qCCritical(lcClientShip)
                    << "Exception in defineSimulator:"
                    << e.what();
            }
            return false;
        }
    });
}

/**
 * @brief Runs the simulator for specified networks
 *
 * Initiates simulation execution for given networks.
 *
 * @param networkNames Networks to run or "*" for all
 * @param byTimeSteps Time steps to run, -1 for unlimited
 * @return True if successful
 */
bool ShipSimulationClient::runSimulator(
    const QStringList &networkNames, double byTimeSteps)
{
    return executeSerializedCommand([&]() {
        QStringList networks = networkNames;
        if (networks.contains("*"))
        {
            CargoNetSim::Backend::Commons::ScopedReadLock
                locker(m_dataAccessMutex);
            networks = m_networkData.keys();
        }
        QJsonObject params;
        QJsonArray  networksArray;
        for (const QString &network : networks)
        {
            networksArray.append(network);
        }
        params["networkNames"] = networksArray;
        params["byTimeSteps"]  = byTimeSteps;
        bool success           = sendCommandAndWait(
            "runSimulator", params,
            {"allshipsreacheddestination"});
        if (m_logger)
        {
            if (success)
            {
                m_logger->log(
                    "Simulator run for "
                        + networks.join(", "),
                    static_cast<int>(m_clientType));
            }
            else
            {
                m_logger->logError(
                    "Failed to run simulator for "
                        + networks.join(", "),
                    static_cast<int>(m_clientType));
            }
        }
        return success;
    });
}

/**
 * @brief Advances simulation by a specific time step
 *
 * Runs the simulation for a specified time step, waiting
 * for simulationAdvanced event instead of completion.
 *
 * @param networkNames Networks to advance or "*" for all
 * @param deltaT Time step in seconds
 * @return True if successful
 */
bool ShipSimulationClient::advanceByTimeStep(
    const QStringList& networkNames,
    double deltaT)
{
    return executeSerializedCommand([&]() {
        QJsonObject params;

        QJsonArray networks;
        for (const QString& name : networkNames)
        {
            networks.append(name);
        }
        params["networkNames"] = networks;
        params["byTimeSteps"] = deltaT;

        // Wait for simulationAdvanced instead of allShipsReachedDestination
        bool success = sendCommandAndWait(
            "runSimulator",
            params,
            {"simulationadvanced", "allshipsreacheddestination"});

        return success;
    });
}

/**
 * @brief Notifies about terminal closure for rerouting
 *
 * Sends notification about a closed terminal to the server.
 *
 * @param terminalId Closed terminal identifier
 * @param alternativeId Alternative terminal for rerouting
 */
void ShipSimulationClient::notifyTerminalClosure(
    const QString& terminalId,
    const QString& alternativeId)
{
    executeSerializedCommand([&]() {
        QJsonObject params;
        params["closedTerminal"] = terminalId;
        params["alternativeTerminal"] = alternativeId;

        return sendCommandAndWait(
            "notifyTerminalClosure",
            params,
            {"terminalClosureAcknowledged"});
    });
}

/**
 * @brief Notifies about terminal reopening
 *
 * Sends notification about a reopened terminal to the server.
 *
 * @param terminalId Reopened terminal identifier
 */
void ShipSimulationClient::notifyTerminalReopened(
    const QString& terminalId)
{
    executeSerializedCommand([&]() {
        QJsonObject params;
        params["terminalId"] = terminalId;

        return sendCommandAndWait(
            "notifyTerminalReopened",
            params,
            {"terminalReopenedAcknowledged"});
    });
}

/**
 * @brief Ends the simulator for specified networks
 *
 * Stops simulation execution for given networks.
 *
 * @param networkNames Networks to end or "*" for all
 * @return True if successful
 */
bool ShipSimulationClient::endSimulator(
    const QStringList &networkNames)
{
    return executeSerializedCommand([&]() {
        QStringList networks = networkNames;
        if (networks.contains("*"))
        {
            CargoNetSim::Backend::Commons::ScopedReadLock
                locker(m_dataAccessMutex);
            networks = m_networkData.keys();
        }
        QJsonObject params;
        QJsonArray  networksArray;
        for (const QString &network : networks)
        {
            networksArray.append(network);
        }
        params["network"] = networksArray;
        bool success      = sendCommandAndWait(
            "endSimulator", params, {"simulationended"});
        if (m_logger)
        {
            if (success)
            {
                m_logger->log(
                    "Simulator ended for "
                        + networks.join(", "),
                    static_cast<int>(m_clientType));
            }
            else
            {
                m_logger->logError(
                    "Failed to end simulator for "
                        + networks.join(", "),
                    static_cast<int>(m_clientType));
            }
        }
        return success;
    });
}

/**
 * @brief Adds ships to an existing simulator
 *
 * Integrates new ships into a running simulation network.
 *
 * @param networkName Target network name
 * @param ships List of Ship pointers to add
 * @param destinationTerminalIds Ship ID to terminal IDs map
 * @return True if successful
 */
bool ShipSimulationClient::addShipsToSimulator(
    const QString &networkName, const QList<Ship *> &ships,
    const QMap<QString, QStringList>
        &destinationTerminalIds)
{
    return executeSerializedCommand([&]() {
        QJsonArray shipsArray;
        for (const auto *ship : ships)
        {
            if (ship)
            {
                shipsArray.append(ship->toJson());
            }
        }
        QJsonObject params;
        params["networkName"] = networkName;
        params["ships"]       = shipsArray;
        bool success          = sendCommandAndWait(
            "addShipsToSimulator", params,
            {"shipaddedtosimulator"});
        if (success)
        {
            CargoNetSim::Backend::Commons::ScopedWriteLock
                locker(m_dataAccessMutex);
            for (auto *ship : ships)
            {
                if (ship)
                {
                    m_loadedShips[ship->getUserId()] = ship;
                    auto terminals =
                        destinationTerminalIds.value(
                            ship->getUserId());
                    m_shipsDestinationTerminals
                        [ship->getUserId()] = terminals;
                }
            }
            if (m_logger)
            {
                m_logger->log(
                    "Ships added to " + networkName,
                    static_cast<int>(m_clientType));
            }
        }
        return success;
    });
}

/**
 * @brief Adds containers to a ship
 *
 * Assigns containers to a ship in a specified network.
 *
 * @param networkName Network name
 * @param shipId Ship identifier
 * @param containers List of Container pointers
 * @return True if successful
 */
bool ShipSimulationClient::addContainersToShip(
    const QString &networkName, const QString &shipId,
    const QList<ContainerCore::Container *> &containers)
{
    return executeSerializedCommand([&]() {
        QJsonArray containersArray;
        for (const auto *container : containers)
        {
            if (container)
            {
                containersArray.append(container->toJson());
            }
        }
        QJsonObject params;
        params["networkName"] = networkName;
        params["shipID"]      = shipId;
        params["containers"]  = containersArray;
        bool success          = sendCommandAndWait(
            "addContainersToShip", params,
            {"containersaddedtoship"});
        if (m_logger)
        {
            if (success)
            {
                m_logger->log(
                    "Containers added to ship " + shipId,
                    static_cast<int>(m_clientType));
            }
            else
            {
                m_logger->logError(
                    "Failed to add containers to " + shipId,
                    static_cast<int>(m_clientType));
            }
        }
        return success;
    });
}

/**
 * @brief Internal method to unload containers
 *
 * Sends an unload command and wait for a response.
 *
 * @param networkName Network name
 * @param shipId Ship identifier
 * @param terminalNames Terminal names for unloading
 * @return True if command is sent successfully
 */
bool ShipSimulationClient::
    unloadContainersFromShipAtTerminalsPrivate(
        const QString &networkName, const QString &shipId,
        const QStringList &terminalNames)
{
    qCInfo(lcClientShip)
        << "ShipSimulationClient::unloadContainersFromShipAtTerminalsPrivate:"
        << "network=" << networkName
        << "shipId=" << shipId
        << "requestedTerminals=" << terminalNames;

    QJsonObject params;
    params["networkName"] = networkName;
    params["shipID"]      = shipId;
    QJsonArray terminalsArray;
    for (const QString &terminal : terminalNames)
    {
        terminalsArray.append(terminal);
    }
    params["terminalNames"] = terminalsArray;

    // Create local event loop
    QEventLoop eventLoop;
    bool       success = false;

    // Connect the event signal to break the local event
    // loop
    connect(this, &SimulationClientBase::eventReceived,
            [this, &eventLoop,
             &success](const QString     &event,
                       const QJsonObject &data) {
                if (normalizeEventName(event)
                    == "containersunloaded")
                {
                    success = true;
                    eventLoop.quit();
                }
            });

    // Send the command without waiting
    if (sendCommand("unloadContainersFromShipAtTerminal",
                    params))
    {
        // Wait for the event with a timeout
        QTimer::singleShot(
            30000, &eventLoop,
            &QEventLoop::quit); // 30-second timeout
        eventLoop.exec();
    }

    if (m_logger)
    {
        if (success)
        {
            m_logger->log("Ship " + shipId + " unloaded",
                          static_cast<int>(m_clientType));
        }
        else
        {
            m_logger->logError(
                "Failed to unload ship " + shipId,
                static_cast<int>(m_clientType));
        }
    }
    return success;
}

/**
 * @brief Unloads containers from a ship at terminals
 *
 * Executes and waits for unloading of containers to
 * terminals.
 *
 * @param networkName Network name
 * @param shipId Ship identifier
 * @param terminalNames Terminal names
 * @return True if successful
 */
bool ShipSimulationClient::
    unloadContainersFromShipAtTerminals(
        const QString &networkName, const QString &shipId,
        const QStringList &terminalNames)
{
    return executeSerializedCommand([&]() {
        QJsonObject params;
        params["networkName"] = networkName;
        params["shipID"]      = shipId;
        QJsonArray terminalsArray;
        for (const QString &terminal : terminalNames)
        {
            terminalsArray.append(terminal);
        }
        params["terminalNames"] = terminalsArray;
        return sendCommandAndWait(
            "unloadContainersFromShipAtTerminal", params,
            {"containersUnloaded"});
    });
}

/**
 * @brief Requests terminal nodes for a network
 *
 * Sends a request for terminal nodes in a specified
 * network.
 *
 * @param networkName Network name
 */
void ShipSimulationClient::getNetworkTerminalNodes(
    const QString &networkName)
{
    executeSerializedCommand([&]() {
        QJsonObject params;
        params["network"] = networkName;
        bool success =
            sendCommand("getNetworkSeaPorts", params);
        if (m_logger)
        {
            m_logger->log("Requested terminal nodes for "
                              + networkName,
                          static_cast<int>(m_clientType));
        }
        return success;
    });
}

/**
 * @brief Requests shortest path between nodes
 *
 * Sends a request for the shortest path in a network.
 *
 * @param networkName Network name
 * @param startNode Start node ID
 * @param endNode End node ID
 */
void ShipSimulationClient::getShortestPath(
    const QString &networkName, const QString &startNode,
    const QString &endNode)
{
    executeSerializedCommand([&]() {
        QJsonObject params;
        params["network"]   = networkName;
        params["startNode"] = startNode;
        params["endNode"]   = endNode;
        bool success =
            sendCommand("getShortestPath", params);
        if (m_logger)
        {
            m_logger->log("Requested shortest path in "
                              + networkName,
                          static_cast<int>(m_clientType));
        }
        return success;
    });
}

/**
 * @brief Retrieves the state of a specific ship
 *
 * Fetches the current state of a ship from stored data.
 *
 * @param networkName Network name
 * @param shipId Ship identifier
 * @return ShipState pointer or nullptr if not found
 */
const ShipState *ShipSimulationClient::getShipState(
    const QString &networkName, const QString &shipId) const
{
    CargoNetSim::Backend::Commons::ScopedReadLock locker(
        m_dataAccessMutex);
    if (!m_shipState.contains(networkName))
    {
        if (m_logger)
        {
            m_logger->log("No ship state for network "
                              + networkName,
                          static_cast<int>(m_clientType));
        }
        return nullptr;
    }
    const auto &states = m_shipState[networkName];
    for (const auto *state : states)
    {
        if (state && state->getShipId() == shipId)
        {
            return state;
        }
    }
    if (m_logger)
    {
        m_logger->log("Ship " + shipId + " not found in "
                          + networkName,
                      static_cast<int>(m_clientType));
    }
    return nullptr;
}

/**
 * @brief Retrieves states of all ships in a network
 *
 * Fetches all ship states for a specified network.
 *
 * @param networkName Network name
 * @return List of ShipState pointers, empty if none
 */
QList<const ShipState *>
ShipSimulationClient::getAllNetworkShipsStates(
    const QString &networkName) const
{
    CargoNetSim::Backend::Commons::ScopedReadLock locker(
        m_dataAccessMutex);
    QList<const ShipState *> statesList;
    if (!m_shipState.contains(networkName))
    {
        if (m_logger)
        {
            m_logger->log("No ship states for "
                              + networkName,
                          static_cast<int>(m_clientType));
        }
        return statesList;
    }
    const auto &states = m_shipState[networkName];
    for (const auto *state : states)
    {
        if (state)
        {
            statesList.append(state);
        }
    }
    return statesList;
}

/**
 * @brief Retrieves states of all ships across networks
 *
 * Fetches ship states for all networks in a mapped
 * structure.
 *
 * @return Map of network names to ShipState pointer lists
 */
QMap<QString, QList<const ShipState *>>
ShipSimulationClient::getAllShipsStates() const
{
    CargoNetSim::Backend::Commons::ScopedReadLock locker(
        m_dataAccessMutex);
    QMap<QString, QList<const ShipState *>> allStates;
    for (auto it = m_shipState.constBegin();
         it != m_shipState.constEnd(); ++it)
    {
        QList<const ShipState *> networkStates;
        for (const auto *state : it.value())
        {
            if (state)
            {
                networkStates.append(state);
            }
        }
        allStates[it.key()] = networkStates;
    }
    return allStates;
}

/**
 * @brief Processes server messages
 *
 * Dispatches incoming messages to appropriate event
 * handlers.
 *
 * @param message JSON message from the server
 */
void ShipSimulationClient::onEventReceived(
    const QString     &normalizedEvent,
    const QJsonObject &message)
{
    if (normalizedEvent == "simulationnetworkloaded")
    {
        onSimulationNetworkLoaded(message);
    }
    else if (normalizedEvent == "simulationcreated")
    {
        onSimulationCreated(message);
    }
    else if (normalizedEvent == "simulationended")
    {
        onSimulationEnded(message);
    }
    else if (normalizedEvent == "simulationadvanced")
    {
        onSimulationAdvanced(message);
    }
    else if (normalizedEvent == "simulationprogressupdate")
    {
        onSimulationProgressUpdate(message);
    }
    else if (normalizedEvent == "shipaddedtosimulator")
    {
        onShipAddedToSimulator(message);
    }
    else if (normalizedEvent == "shipreacheddestination")
    {
        onShipReachedDestination(message);
    }
    else if (normalizedEvent
             == "allshipsreacheddestination")
    {
        onAllShipsReachedDestination(message);
    }
    else if (normalizedEvent
             == "simulationresultsavailable")
    {
        onSimulationResultsAvailable(message);
    }
    else if (normalizedEvent == "shipstate")
    {
        onShipStateAvailable(message);
    }
    else if (normalizedEvent == "simulatorstate")
    {
        onSimulatorStateAvailable(message);
    }
    else if (normalizedEvent == "containersaddedtoship")
    {
        onContainersAdded(message);
    }
    else if (normalizedEvent == "containersunloaded")
    {
        onContainersUnloaded(message);
    }
    else if (normalizedEvent == "shipreachedseaport")
    {
        onShipReachedSeaport(message);
    }
    else if (normalizedEvent == "erroroccurred")
    {
        onErrorOccurred(message);
    }
    else if (normalizedEvent == "serverreset")
    {
        onServerReset();
    }
    else if (normalizedEvent == "simulationpaused")
    {
        onSimulationPaused(message);
    }
    else if (normalizedEvent == "simulationresumed")
    {
        onSimulationResumed(message);
    }
    else if (normalizedEvent == "simulationrestarted")
    {
        onSimulationRestarted(message);
    }
    else
    {
        if (m_logger)
        {
            m_logger->log("Unrecognized event: "
                              + normalizedEvent,
                          static_cast<int>(m_clientType));
        }
        else
        {
            qCWarning(lcClientShip)
                << "Unrecognized event:" << normalizedEvent;
        }
    }
}

/**
 * @brief Handles simulation network loaded event
 *
 * Logs the event when a simulation network is loaded.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationNetworkLoaded(
    const QJsonObject &message)
{
    if (m_logger)
    {
        m_logger->log("Simulation network loaded",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation network loaded.";
    }
    // No shared state modification, no mutex needed
}

/**
 * @brief Handles simulation created event
 *
 * Initializes network data when a simulation is created.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationCreated(
    const QJsonObject &message)
{
    QString networkName =
        message.value("networkName").toString();
    // Mutex needed because we're modifying m_networkData
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_dataAccessMutex);
    m_networkData[networkName] =
        QList<SimulationResults *>();
    if (m_logger)
    {
        m_logger->log("Simulation created for "
                          + networkName,
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation created.";
    }
}

/**
 * @brief Handles simulation paused event
 *
 * Logs the event when a simulation is paused.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationPaused(
    const QJsonObject &message)
{
    if (m_logger)
    {
        m_logger->log("Simulation paused",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation paused.";
    }
}

/**
 * @brief Handles simulation resumed event
 *
 * Logs the event when a simulation is resumed.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationResumed(
    const QJsonObject &message)
{
    if (m_logger)
    {
        m_logger->log("Simulation resumed",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation resumed.";
    }
}

/**
 * @brief Handles simulation restarted event
 *
 * Logs the event when a simulation is restarted.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationRestarted(
    const QJsonObject &message)
{
    if (m_logger)
    {
        m_logger->log("Simulation restarted",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation restarted.";
    }
}

/**
 * @brief Handles simulation ended event
 *
 * Logs the event when a simulation ends.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationEnded(
    const QJsonObject &message)
{
    if (m_logger)
    {
        m_logger->log("Simulation ended",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation ended.";
    }
}

/**
 * @brief Handles simulation advanced event
 *
 * Logs progress when a simulation advances in time.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationAdvanced(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    double newTime =
        message.value("newSimulationTime").toDouble();
    QJsonObject networkProgresses =
        message.value("networkNamesProgress").toObject();
    if (!networkProgresses.isEmpty())
    {
        double      totalProgress = 0.0;
        QStringList networks;
        for (auto it = networkProgresses.constBegin();
             it != networkProgresses.constEnd(); ++it)
        {
            networks.append(it.key());
            totalProgress += it.value().toDouble();
        }
        double average =
            networks.isEmpty()
                ? 0.0
                : totalProgress / networks.size();
        if (m_logger)
        {
            m_logger->log("Simulation advanced to time: "
                              + QString::number(newTime),
                          static_cast<int>(m_clientType));
            m_logger->log("Simulations advanced for "
                              + networks.join(", "),
                          static_cast<int>(m_clientType));
        }
        else
        {
            qCDebug(lcClientShip) << "Simulation advanced to time:"
                     << newTime;
            qCDebug(lcClientShip) << "Simulations advanced for"
                     << networks.join(", ");
        }
    }
    else
    {
        if (m_logger)
        {
            m_logger->log(
                "Invalid 'networkNamesProgress' in message",
                static_cast<int>(m_clientType));
        }
        else
        {
            qCWarning(lcClientShip) << "Invalid or missing "
                          "'networkNamesProgress'";
        }
    }
}

/**
 * @brief Handles simulation progress update event
 *
 * Updates progress logging when simulation progress
 * changes.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationProgressUpdate(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    double progress =
        message.value("newProgress").toDouble();
    if (m_logger)
    {
        m_logger->updateProgress(
            progress, static_cast<int>(m_clientType));
    }
}

/**
 * @brief Handles ship added to simulator event
 *
 * Logs when a ship is added to the simulator.
 *
 * @param message Event data
 */
void ShipSimulationClient::onShipAddedToSimulator(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QString shipId = message.value("shipID").toString();
    if (m_logger)
    {
        m_logger->log("Ship " + shipId
                          + " added to simulator",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Ship" << shipId
                 << "added to simulator.";
    }
}

/**
 * @brief Handles all ships reached destination event
 *
 * Logs when all ships reach their destinations.
 *
 * @param message Event data
 */
void ShipSimulationClient::onAllShipsReachedDestination(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QString networkName =
        message.value("networkName").toString();
    if (m_logger)
    {
        m_logger->log("All ships reached destination in "
                          + networkName,
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip)
            << "All ships reached destination of network:"
            << networkName;
    }
}

/**
 * @brief Handles ship reached destination event
 *
 * Updates ship state and unloads containers when a ship
 * arrives.
 *
 * @param message Event data
 */
void ShipSimulationClient::onShipReachedDestination(
    const QJsonObject &message)
{
    QStringList shipIds;
    QMap<QString, QStringList>
        unloadOperations; // Store operations to perform
                          // after releasing lock

    // First critical section: Extract data and prepare
    // operations
    {
        // Mutex needed because we're modifying m_shipState
        CargoNetSim::Backend::Commons::ScopedWriteLock
                    locker(m_dataAccessMutex);
        QJsonObject shipStatus =
            message.value("state").toObject();

        for (auto it = shipStatus.constBegin();
             it != shipStatus.constEnd(); ++it)
        {
            QString networkName = it.key();
            if (!m_shipState.contains(networkName))
            {
                m_shipState[networkName] =
                    QList<ShipState *>();
            }

            QJsonObject networkStatus =
                it.value().toObject();
            if (networkStatus.contains("shipStates"))
            {
                QJsonObject shipData =
                    networkStatus.value("shipStates")
                        .toObject();
                QString shipId =
                    shipData.value("shipID").toString();
                int containersCount =
                    shipData.value("containersCount")
                        .toInt();
                QStringList terminalIds =
                    m_shipsDestinationTerminals.value(
                        shipId);
                qCInfo(lcClientShip)
                    << "ShipSimulationClient::onShipReachedDestination:"
                    << "network=" << networkName
                    << "shipId=" << shipId
                    << "containersCount=" << containersCount
                    << "cachedDestinationTerminals="
                    << terminalIds;

                // Create and store ship state
                ShipState *shipState =
                    new ShipState(shipData);
                m_shipState[networkName].append(shipState);
                shipIds.append(shipId);

                // Store the unload operation for execution
                // outside the lock
                if (!terminalIds.isEmpty())
                {
                    unloadOperations[networkName + ":"
                                     + shipId] =
                        terminalIds;
                }
            }
        }
    }

    // Second section: Perform unload operations without
    // holding the lock
    for (auto it = unloadOperations.constBegin();
         it != unloadOperations.constEnd(); ++it)
    {
        QStringList parts = it.key().split(":");
        if (parts.size() == 2)
        {
            QString     networkName = parts[0];
            QString     shipId      = parts[1];
            QStringList terminalIds = it.value();

            unloadContainersFromShipAtTerminalsPrivate(
                networkName, shipId, terminalIds);
        }
    }

    // Log the result
    if (m_logger)
    {
        m_logger->log("Ships [" + shipIds.join(", ")
                          + "] reached destinations",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Ships [" << shipIds.join(", ")
                 << "] reached destinations";
    }
}

/**
 * @brief Handles ship reached seaport event
 *
 * Unloads containers when a ship reaches a seaport.
 *
 * @param message Event data
 */
void ShipSimulationClient::onShipReachedSeaport(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QString terminalId =
        message.value("seaPortCode").toString();
    int containersCount =
        message.value("containersCount").toInt();
    QString networkName =
        message.value("networkName").toString();
    QString shipId = message.value("shipID").toString();
    qCInfo(lcClientShip)
        << "ShipSimulationClient::onShipReachedSeaport:"
        << "network=" << networkName
        << "shipId=" << shipId
        << "seaPortCode=" << terminalId
        << "containersCount=" << containersCount;
    unloadContainersFromShipAtTerminalsPrivate(
        networkName, shipId, QStringList{terminalId});

    if (m_logger)
    {
        m_logger->log("Ship " + shipId + " reached seaport "
                          + terminalId,
                      static_cast<int>(m_clientType));
    }
}

/**
 * @brief Handles containers unloaded event
 *
 * Logs when containers are unloaded from a ship.
 *
 * @param message Event data
 */
void ShipSimulationClient::onContainersUnloaded(
    const QJsonObject &message)
{
    QString portName =
        message.value("portName").toString();
    QString networkName =
        message.value("networkName").toString();
    QJsonArray containers =
        message.value("containers").toArray();

    QJsonObject   containersObj{{"containers", containers}};
    QJsonDocument doc(containersObj);
    QString containersJson =
        doc.toJson(QJsonDocument::Compact);

    QString fullTerminalID =
        QString(networkName + "_" + portName);

    double currentTime =
        m_simulationTime->getCurrentTime();

    qCInfo(lcClientShip)
        << "ShipSimulationClient::onContainersUnloaded:"
        << "network=" << networkName
        << "portName=" << portName
        << "fullTerminalID=" << fullTerminalID
        << "containerCount=" << containers.size()
        << "firstContainer=" << previewContainers(containers);

    if (portName.isEmpty())
    {
        qCWarning(lcClientShip)
            << "ShipSimulationClient::onContainersUnloaded:"
            << "empty portName from ShipNetSim for network"
            << networkName
            << "containersCount=" << containers.size();
    }

    if (m_terminalClient)
    {
        const bool addSuccess = m_terminalClient->addContainers(
            fullTerminalID, containersJson,
            currentTime, "Ship");
        qCInfo(lcClientShip)
            << "ShipSimulationClient::onContainersUnloaded:"
            << "terminal handoff result=" << addSuccess;
    }

    if (m_logger)
    {
        m_logger->log("Containers unloaded at port: "
                          + portName,
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Containers unloaded at port:"
                 << portName;
    }
}

/**
 * @brief Handles simulation results available event
 *
 * Logs when simulation results are available.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulationResultsAvailable(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QJsonObject results =
        message.value("results").toObject();
    if (m_logger)
    {
        m_logger->log("Simulation results available",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulation results available";
    }
}

/**
 * @brief Handles ship state available event
 *
 * Updates the ship state data structure.
 *
 * @param message Event data
 */
void ShipSimulationClient::onShipStateAvailable(
    const QJsonObject &message)
{
    // Mutex needed because we're modifying m_shipState
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_dataAccessMutex);
    QJsonObject shipState =
        message.value("state").toObject();

    // Implement state handling here
    for (auto it = shipState.constBegin();
         it != shipState.constEnd(); ++it)
    {
        QString networkName = it.key();

        // Ensure the network exists in our map
        if (!m_shipState.contains(networkName))
        {
            m_shipState[networkName] = QList<ShipState *>();
        }

        // Process ship states in this network
        QJsonObject networkStatus = it.value().toObject();
        if (networkStatus.contains("shipStates"))
        {
            QJsonObject shipData =
                networkStatus.value("shipStates")
                    .toObject();
            QString shipId =
                shipData.value("shipID").toString();

            // Check if we already have a state for this
            // ship and remove it to avoid memory leaks
            auto &shipStates = m_shipState[networkName];
            for (int i = 0; i < shipStates.size(); ++i)
            {
                if (shipStates[i]->getShipId() == shipId)
                {
                    // Delete the old state and remove it
                    // from the list
                    delete shipStates[i];
                    shipStates.removeAt(i);
                    break;
                }
            }

            // Create new ship state from the received data
            ShipState *shipState = new ShipState(shipData);

            // Add to our state collection
            shipStates.append(shipState);
        }
    }

    if (m_logger)
    {
        m_logger->log("Ship state available",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Ship state available";
    }
}

/**
 * @brief Handles simulator state available event
 *
 * Logs when the simulator state becomes available.
 *
 * @param message Event data
 */
void ShipSimulationClient::onSimulatorStateAvailable(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QJsonObject simulatorState =
        message.value("state").toObject();
    if (m_logger)
    {
        m_logger->log("Simulator state available",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Simulator state available";
    }
}

/**
 * @brief Handles error occurred event
 *
 * Logs an error message from the server.
 *
 * @param message Event data
 */
void ShipSimulationClient::onErrorOccurred(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QString errorMessage =
        message.value("errorMessage").toString();
    if (m_logger)
    {
        m_logger->logError("Error occurred: "
                               + errorMessage,
                           static_cast<int>(m_clientType));
    }
    else
    {
        qCCritical(lcClientShip) << "Error occurred:" << errorMessage;
    }
}

/**
 * @brief Handles server reset event
 *
 * Logs when the server is successfully reset.
 */
void ShipSimulationClient::onServerReset()
{
    // No shared state modification, no mutex needed
    if (m_logger)
    {
        m_logger->log("Server reset successfully",
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Server Reset Successfully";
    }
}

/**
 * @brief Handles containers added event
 *
 * Logs when containers are added to a ship.
 *
 * @param message Event data
 */
void ShipSimulationClient::onContainersAdded(
    const QJsonObject &message)
{
    // No shared state modification, no mutex needed
    QString network =
        message.value("networkName").toString();
    QString shipId = message.value("shipID").toString();
    if (m_logger)
    {
        m_logger->log("Containers added to ship " + shipId
                          + " on " + network,
                      static_cast<int>(m_clientType));
    }
    else
    {
        qCInfo(lcClientShip) << "Containers added to ship" << shipId
                 << "on network" << network;
    }
}

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim
