/**
 * @file TruckSimulationManager.h
 * @brief Manages truck simulation clients
 * @author Ahmed Aredah
 * @date April 11, 2025
 */

#pragma once

#include "Backend/Commons/LoggerInterface.h"
#include "Backend/Commons/Units.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include "TruckSimulationClient.h"
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <memory>

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

/**
 * @class ClientConfiguration
 * @brief Configuration parameters for creating a truck
 * simulation client
 */
struct ClientConfiguration
{
    QString exePath; ///< Path to the simulation executable
    QString host = "localhost"; ///< RabbitMQ host
    int     port = 5672;        ///< RabbitMQ port
    QString masterFilePath;     ///< Path to master
                                ///< configuration file
    double simTime =
        CargoNetSim::Backend::Units::toSeconds(
            CargoNetSim::Backend::Units::hours(1.0))
            .value(); ///< Simulation duration in seconds
    QMap<QString, QVariant>
        configUpdates; ///< Custom configuration parameters
    QStringList
        argsUpdates; ///< Additional command-line arguments

    bool isValid() const
    {
        return !exePath.isEmpty()
               && !masterFilePath.isEmpty();
    }
};

/**
 * @class TruckSimulationManager
 * @brief Manages multiple truck simulation clients
 *
 * Thread-safe manager that handles the lifecycle of truck
 * simulation clients, including creation, configuration,
 * thread assignment, and communication.
 */
class TruckSimulationManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit TruckSimulationManager(
        QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~TruckSimulationManager();

    /**
     * @brief Initialize the manager
     * @param simulationTime Global simulation time
     * @param logger Logger interface
     */
    void initializeManager(
        SimulationTime           *simulationTime,
        TerminalSimulationClient *terminalClient,
        LoggerInterface          *logger);

    /**
     * @brief Forcefully resets all clients and their
     * processes
     *
     * This method will:
     * 1. Kill all running client processes immediately
     * 2. Terminate all client threads
     * 3. Delete all client instances
     * 4. Clear all tracked client data
     *
     * @note Use with caution as this will forcefully
     * terminate all simulations without proper cleanup
     * @return True if reset was successful
     */
    bool resetServer();

    /**
     * @brief Creates a new client with the given
     * configuration
     * @param networkName Unique network name identifier
     * @param config Client configuration
     * @return True if client was created successfully
     * @throws std::invalid_argument If networkName is empty
     * or already exists
     */
    bool createClient(const QString &networkName,
                      const ClientConfiguration &config);

    /**
     * @brief Removes a client
     * @param networkName Network name of client to remove
     * @return True if client was found and removed
     */
    bool removeClient(const QString &networkName);

    /**
     * @brief Renames a client
     * @param oldNetworkName Current network name
     * @param newNetworkName New network name
     * @return True if client was renamed successfully
     * @throws std::invalid_argument If newNetworkName
     * already exists
     */
    bool renameClient(const QString &oldNetworkName,
                      const QString &newNetworkName);

    /**
     * @brief Updates a client's configuration
     * @param networkName Network name of client to update
     * @param config New configuration
     * @return True if client was updated successfully
     */
    bool
    updateClientConfig(const QString &networkName,
                       const ClientConfiguration &config);

    /**
     * @brief Gets a list of all client network names
     * @return List of network names
     */
    QStringList getAllClientNames() const;

    /**
     * @brief Gets the configuration for a specific client
     * @param networkName Network name of client
     * @return Client configuration
     * @throws std::invalid_argument If client doesn't exist
     */
    ClientConfiguration
    getClientConfig(const QString &networkName) const;

    /**
     * @brief Run simulation synchronously for specified
     * networks
     * @param networkNames List of network names to run, "*"
     * for all
     * @return True if simulation started successfully
     */
    bool runSimulationSync(const QStringList &networkNames);

    /**
     * @brief Run simulation asynchronously for specified
     * networks
     * @param networkNames List of network names to run, "*"
     * for all
     * @return True if simulation started successfully
     */
    bool
    runSimulationAsync(const QStringList &networkNames);

    /**
     * @brief Check if any client is connected to RabbitMQ
     * @return True if at least one client is connected
     */
    bool isConnected() const;

    /**
     * @brief Check if any client has command queue
     * consumers
     * @return True if at least one client has consumers
     */
    bool hasCommandQueueConsumers() const;

    /**
     * @brief Gets a RabbitMQ handler from any connected
     * client
     * @return Pointer to a RabbitMQ handler or nullptr if
     * none
     */
    RabbitMQHandler *getRabbitMQHandler() const;

    /**
     * @brief Get overall progress across all simulations
     * @return Average progress percentage (0-100)
     */
    double getOverallProgress() const;

    /**
     * @brief Access a specific client (for advanced
     * operations)
     * @param networkName Network name of the client
     * @return Pointer to the client or nullptr if not found
     * @note This allows direct interaction when needed, use
     * with caution
     */
    TruckSimulationClient *
    getClient(const QString &networkName);

private:
    /**
     * @brief Check if simulations should continue
     * @param networkNames List of network names to check
     * @return True if any simulation should continue
     */
    bool keepGoing(const QStringList &networkNames) const;

    /**
     * @brief Run one synchronization step
     * @param networkNames List of network names to sync
     */
    void syncGoOnce(const QStringList &networkNames);

    /**
     * @brief Create and initialize a new thread for a
     * client
     * @param networkName Network name for the client
     * @return Pointer to the created thread
     */
    QThread *createClientThread(const QString &networkName);

    /**
     * @brief Internal method to initialize a client in its
     * thread
     * @param client The client to initialize
     * @param networkName Network name for logging
     */
    void
    initializeClientInThread(TruckSimulationClient *client,
                             const QString &networkName);

    /** Global simulation time reference */
    SimulationTime *m_defaultSimulationTime = nullptr;

    /** Global logger reference */
    LoggerInterface *m_defaultLogger = nullptr;

    /** Global terminal client reference */
    TerminalSimulationClient *m_defaultTerminalClient =
        nullptr;

    /** Map of network names to client instances */
    QMap<QString, TruckSimulationClient *> m_clients;

    /** Map of network names to dedicated threads */
    QMap<QString, QThread *> m_clientThreads;

    /** Map of network names to client configurations */
    QMap<QString, ClientConfiguration> m_clientConfigs;

    /** Mutex for thread-safe access to internal data */
    mutable QReadWriteLock m_mutex;

    /** Wait interval for synchronous simulation (seconds)
     */
    static constexpr double WAIT_INTERVAL = 0.1;

signals:
    /**
     * @brief Signal emitted when progress updates
     * @param progress Progress percentage (0-100)
     */
    void progressUpdated(double progress) const;

    /**
     * @brief Signal emitted when all clients are reset
     */
    void clientsReset();

    /**
     * @brief Signal emitted when a client is added
     * @param networkName Network name of the added client
     */
    void clientAdded(const QString &networkName);

    /**
     * @brief Signal emitted when a client is removed
     * @param networkName Network name of the removed client
     */
    void clientRemoved(const QString &networkName);

    /**
     * @brief Signal emitted when a client is renamed
     * @param oldNetworkName Previous network name
     * @param newNetworkName New network name
     */
    void clientRenamed(const QString &oldNetworkName,
                       const QString &newNetworkName);

    /**
     * @brief Signal emitted when a client is updated
     * @param networkName Network name of the updated client
     */
    void clientUpdated(const QString &networkName);

    void tripEnded(const QString &networkName,
                   const QString &tripId);

    void tripEndedWithData(const TripEndData &tripData);
};

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
