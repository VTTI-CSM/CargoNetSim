/**
 * @file TruckSimulationManager.cpp
 * @brief Implements truck simulation manager
 * @author Ahmed Aredah
 * @date March 21, 2025
 */

#include "TruckSimulationManager.h"
#include "Backend/Commons/LogCategories.h"
#include <QThread>
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

TruckSimulationManager::TruckSimulationManager(
    QObject *parent)
    : QObject(parent)
{
}

TruckSimulationManager::~TruckSimulationManager()
{
    Commons::ScopedWriteLock locker(m_mutex);

    // First, stop all threads
    for (auto *thread : m_clientThreads.values())
    {
        if (thread && thread->isRunning())
        {
            thread->quit();
            thread->wait(3000); // Wait up to 3 seconds

            if (thread->isRunning())
            {
                thread->terminate();
                thread->wait(1000);
            }
        }
    }

    // Now delete clients (they should be in their threads)
    for (auto *client : m_clients.values())
    {
        delete client;
    }

    // Finally delete threads
    for (auto *thread : m_clientThreads.values())
    {
        delete thread;
    }

    m_clients.clear();
    m_clientThreads.clear();
    m_clientConfigs.clear();
}

void TruckSimulationManager::initializeManager(
    SimulationTime           *simulationTime,
    TerminalSimulationClient *terminalClient,
    LoggerInterface          *logger)
{
    qCInfo(lcClientTruck)
        << "TruckSimulationManager::initializeManager:"
        << "setting up manager dependencies";
    Commons::ScopedWriteLock locker(m_mutex);
    m_defaultSimulationTime = simulationTime;
    m_defaultTerminalClient = terminalClient;
    m_defaultLogger         = logger;
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::initializeManager:"
        << "completed, simulationTime="
        << (simulationTime != nullptr)
        << "terminalClient="
        << (terminalClient != nullptr)
        << "logger=" << (logger != nullptr);
}

bool TruckSimulationManager::resetServer()
{
    qCInfo(lcClientTruck)
        << "TruckSimulationManager::resetServer:"
        << "initiating force reset of all clients";

    // First, get copies of client pointers to avoid locks
    // during force kill
    QList<TruckSimulationClient *> clientsToKill;
    QList<QThread *>               threadsToKill;
    QStringList                    networkNames;

    // Get all client and thread references with a read lock
    {
        Commons::ScopedReadLock locker(m_mutex);
        clientsToKill = m_clients.values();
        threadsToKill = m_clientThreads.values();
        networkNames  = m_clients.keys();
    }

    qCDebug(lcClientTruck)
        << "TruckSimulationManager::resetServer:"
        << "clients to kill:" << clientsToKill.size()
        << "networks:" << networkNames;

    // Force kill all clients' processes first (without
    // holding a lock)
    for (auto *client : clientsToKill)
    {
        if (client)
        {
            // Force kill all running processes in the
            // client
            for (const QString &name : networkNames)
            {
                try
                {
                    // End simulator sends end command and
                    // terminates process
                    client->endSimulator({name});
                }
                catch (...)
                {
                    qCWarning(lcClientTruck)
                        << "TruckSimulationManager::"
                           "resetServer:"
                        << "exception during endSimulator"
                        << "for network:" << name;
                }
            }
        }
    }

    // Now terminate all threads with a short timeout
    for (auto *thread : threadsToKill)
    {
        if (thread && thread->isRunning())
        {
            thread->quit();
            // Only wait briefly for clean exit
            if (!thread->wait(500))
            {
                qCWarning(lcClientTruck)
                    << "TruckSimulationManager::"
                       "resetServer:"
                    << "thread did not exit cleanly,"
                    << "forcing terminate";
                // Force kill the thread if it doesn't exit
                // cleanly
                thread->terminate();
                thread->wait(100);
            }
        }
    }

    // Finally delete everything with a write lock
    {
        Commons::ScopedWriteLock locker(m_mutex);

        // Delete all clients
        for (auto *client : m_clients.values())
        {
            delete client;
        }

        // Delete all threads
        for (auto *thread : m_clientThreads.values())
        {
            delete thread;
        }

        // Clear all tracking data
        m_clients.clear();
        m_clientThreads.clear();
        m_clientConfigs.clear();

        if (m_defaultLogger)
        {
            m_defaultLogger->log(
                "TruckSimulationManager: Force reset "
                "completed - all clients terminated",
                static_cast<int>(ClientType::TruckClient));
        }
    }

    qCInfo(lcClientTruck)
        << "TruckSimulationManager::resetServer:"
        << "force reset completed";

    // Notify about the reset
    emit clientsReset();

    return true;
}

bool TruckSimulationManager::createClient(
    const QString             &networkName,
    const ClientConfiguration &config)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::createClient:"
        << "networkName=" << networkName
        << "host=" << config.host
        << "port=" << config.port;

    if (networkName.isEmpty())
    {
        throw std::invalid_argument(
            "Network name cannot be empty");
    }

    if (!config.isValid())
    {
        throw std::invalid_argument(
            "Invalid client configuration");
    }

    // Check if client already exists
    {
        Commons::ScopedReadLock locker(m_mutex);
        if (m_clients.contains(networkName))
        {
            throw std::invalid_argument(
                "Network name already exists");
        }
    }

    // Create a new thread for this client
    QThread *clientThread = createClientThread(networkName);

    // Create the client (not yet moved to thread)
    TruckSimulationClient *client =
        new TruckSimulationClient(config.exePath, nullptr,
                                  config.host, config.port);

    connect(client, &TruckSimulationClient::tripEnded,
            this, &TruckSimulationManager::tripEnded);
    connect(client, &TruckSimulationClient::tripEndedWithData,
            this, &TruckSimulationManager::tripEndedWithData);

    // Store client and configuration
    {
        Commons::ScopedWriteLock locker(m_mutex);
        m_clients[networkName]       = client;
        m_clientConfigs[networkName] = config;
        m_clientThreads[networkName] = clientThread;
    }

    // Move client to its thread
    client->moveToThread(clientThread);

    // Initialize the client in its thread
    initializeClientInThread(client, networkName);

    // Start the thread
    clientThread->start();

    // Define simulator (this will be executed in the
    // client's thread)
    bool success = client->defineSimulator(
        networkName, config.masterFilePath, config.simTime,
        config.configUpdates, config.argsUpdates);

    if (success)
    {
        qCInfo(lcClientTruck)
            << "TruckSimulationManager::createClient:"
            << "client created successfully"
            << "networkName=" << networkName;
        emit clientAdded(networkName);
    }
    else
    {
        qCWarning(lcClientTruck)
            << "TruckSimulationManager::createClient:"
            << "defineSimulator failed, removing client"
            << "networkName=" << networkName;
        // Clean up on failure
        removeClient(networkName);
    }

    return success;
}

bool TruckSimulationManager::removeClient(
    const QString &networkName)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::removeClient:"
        << "networkName=" << networkName;

    QThread               *thread = nullptr;
    TruckSimulationClient *client = nullptr;

    // Get references with read lock
    {
        Commons::ScopedReadLock locker(m_mutex);
        if (!m_clients.contains(networkName))
        {
            qCWarning(lcClientTruck)
                << "TruckSimulationManager::removeClient:"
                << "client not found"
                << "networkName=" << networkName;
            return false;
        }

        client = m_clients[networkName];
        thread = m_clientThreads[networkName];
    }

    // End any running simulations
    client->endSimulator({networkName});

    // Remove from collections with write lock
    {
        Commons::ScopedWriteLock locker(m_mutex);
        m_clients.remove(networkName);
        m_clientThreads.remove(networkName);
        m_clientConfigs.remove(networkName);
    }

    // Stop and clean up thread
    if (thread)
    {
        thread->quit();
        if (!thread->wait(3000))
        {
            thread->terminate();
            thread->wait(1000);
        }
        delete thread;
    }

    // Delete the client
    delete client;

    qCDebug(lcClientTruck)
        << "TruckSimulationManager::removeClient:"
        << "client removed successfully"
        << "networkName=" << networkName;
    emit clientRemoved(networkName);
    return true;
}

bool TruckSimulationManager::renameClient(
    const QString &oldNetworkName,
    const QString &newNetworkName)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::renameClient:"
        << "oldName=" << oldNetworkName
        << "newName=" << newNetworkName;

    if (newNetworkName.isEmpty())
    {
        throw std::invalid_argument(
            "New network name cannot be empty");
    }

    TruckSimulationClient *client = nullptr;
    QThread               *thread = nullptr;
    ClientConfiguration    config;

    // Check validity and gather data with read lock
    {
        Commons::ScopedReadLock locker(m_mutex);

        if (!m_clients.contains(oldNetworkName))
        {
            qCWarning(lcClientTruck)
                << "TruckSimulationManager::renameClient:"
                << "old client not found"
                << "oldName=" << oldNetworkName;
            return false;
        }

        if (m_clients.contains(newNetworkName))
        {
            throw std::invalid_argument(
                "New network name already exists");
        }

        client = m_clients[oldNetworkName];
        thread = m_clientThreads[oldNetworkName];
        config = m_clientConfigs[oldNetworkName];
    }

    // First, end any simulations with the old name
    client->endSimulator({oldNetworkName});

    // Update maps with write lock
    {
        Commons::ScopedWriteLock locker(m_mutex);

        // Remove old entries
        m_clients.remove(oldNetworkName);
        m_clientThreads.remove(oldNetworkName);
        m_clientConfigs.remove(oldNetworkName);

        // Add with new name
        m_clients[newNetworkName]       = client;
        m_clientThreads[newNetworkName] = thread;
        m_clientConfigs[newNetworkName] = config;
    }

    // Define simulator with new name
    bool success = client->defineSimulator(
        newNetworkName, config.masterFilePath,
        config.simTime, config.configUpdates,
        config.argsUpdates);

    if (success)
    {
        qCDebug(lcClientTruck)
            << "TruckSimulationManager::renameClient:"
            << "rename successful"
            << "oldName=" << oldNetworkName
            << "newName=" << newNetworkName;
        emit clientRenamed(oldNetworkName, newNetworkName);
    }
    else
    {
        qCWarning(lcClientTruck)
            << "TruckSimulationManager::renameClient:"
            << "defineSimulator failed for new name"
            << "newName=" << newNetworkName;
    }

    return success;
}

bool TruckSimulationManager::updateClientConfig(
    const QString             &networkName,
    const ClientConfiguration &config)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::updateClientConfig:"
        << "networkName=" << networkName;

    if (!config.isValid())
    {
        throw std::invalid_argument(
            "Invalid client configuration");
    }

    TruckSimulationClient *client = nullptr;

    // Get client with read lock
    {
        Commons::ScopedReadLock locker(m_mutex);
        if (!m_clients.contains(networkName))
        {
            qCWarning(lcClientTruck)
                << "TruckSimulationManager::"
                   "updateClientConfig:"
                << "client not found"
                << "networkName=" << networkName;
            return false;
        }

        client = m_clients[networkName];
    }

    // End any running simulations
    client->endSimulator({networkName});

    // Update configuration with write lock
    {
        Commons::ScopedWriteLock locker(m_mutex);
        m_clientConfigs[networkName] = config;
    }

    // Define simulator with updated config
    bool success = client->defineSimulator(
        networkName, config.masterFilePath, config.simTime,
        config.configUpdates, config.argsUpdates);

    if (success)
    {
        qCDebug(lcClientTruck)
            << "TruckSimulationManager::"
               "updateClientConfig:"
            << "config updated successfully"
            << "networkName=" << networkName;
        emit clientUpdated(networkName);
    }
    else
    {
        qCWarning(lcClientTruck)
            << "TruckSimulationManager::"
               "updateClientConfig:"
            << "defineSimulator failed after config update"
            << "networkName=" << networkName;
    }

    return success;
}

QStringList
TruckSimulationManager::getAllClientNames() const
{
    Commons::ScopedReadLock locker(m_mutex);
    return m_clients.keys();
}

ClientConfiguration TruckSimulationManager::getClientConfig(
    const QString &networkName) const
{
    Commons::ScopedReadLock locker(m_mutex);

    if (!m_clientConfigs.contains(networkName))
    {
        throw std::invalid_argument(
            "Client does not exist");
    }

    return m_clientConfigs[networkName];
}

bool TruckSimulationManager::runSimulationSync(
    const QStringList &networkNames)
{
    qCInfo(lcClientTruck)
        << "TruckSimulationManager::runSimulationSync:"
        << "starting sync simulation"
        << "networks=" << networkNames;

    while (keepGoing(networkNames))
    {
        syncGoOnce(networkNames);
        QThread::msleep(
            static_cast<long>(WAIT_INTERVAL * 1000));
    }

    qCInfo(lcClientTruck)
        << "TruckSimulationManager::runSimulationSync:"
        << "sync simulation completed"
        << "networks=" << networkNames;
    return true;
}

bool TruckSimulationManager::runSimulationAsync(
    const QStringList &networkNames)
{
    qCInfo(lcClientTruck)
        << "TruckSimulationManager::runSimulationAsync:"
        << "starting async simulation"
        << "networks=" << networkNames;

    QStringList effectiveNames = networkNames;

    // If wildcard, get all client names
    if (networkNames.contains("*"))
    {
        Commons::ScopedReadLock locker(m_mutex);
        effectiveNames = m_clients.keys();
    }

    bool allSucceeded = true;

    // Process each network in the list
    for (const QString &name : effectiveNames)
    {
        TruckSimulationClient *client = nullptr;

        // Get client with read lock
        {
            Commons::ScopedReadLock locker(m_mutex);
            if (m_clients.contains(name))
            {
                client = m_clients[name];
            }
        }

        // If client exists, run simulator
        if (client)
        {
            bool success = client->runSimulator({name});
            allSucceeded = allSucceeded && success;
        }
        else
        {
            qCWarning(lcClientTruck)
                << "TruckSimulationManager::"
                   "runSimulationAsync:"
                << "client not found for network"
                << "name=" << name;
            allSucceeded = false;
        }
    }

    qCInfo(lcClientTruck)
        << "TruckSimulationManager::runSimulationAsync:"
        << "async simulation dispatched"
        << "allSucceeded=" << allSucceeded;
    return allSucceeded;
}

void TruckSimulationManager::syncGoOnce(
    const QStringList &networkNames)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::syncGoOnce:"
        << "networks=" << networkNames;

    double                                 maxTime = 0.0;
    QStringList                            effectiveNames;
    QMap<QString, double>                  currentTimes;
    QMap<QString, TruckSimulationClient *> relevantClients;

    // Collect data with read lock
    {
        Commons::ScopedReadLock locker(m_mutex);
        effectiveNames = networkNames.contains("*")
                             ? m_clients.keys()
                             : networkNames;

        // Calculate max time across all clients
        for (const QString &name : effectiveNames)
        {
            if (m_clients.contains(name))
            {
                double time =
                    m_clients[name]->getProgressPercentage(
                        name)
                    * m_clients[name]->getSimulationTime(
                        name)
                    / 100.0;
                maxTime               = qMax(maxTime, time);
                currentTimes[name]    = time;
                relevantClients[name] = m_clients[name];
            }
        }
    }

    // Process without holding the lock
    for (const QString &name : effectiveNames)
    {
        if (relevantClients.contains(name))
        {
            double current = currentTimes[name];
            if (current >= maxTime)
            {
                relevantClients[name]->runSimulator({name});
                return; // Exit early after sending command
            }
        }
    }
}

bool TruckSimulationManager::keepGoing(
    const QStringList &networkNames) const
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::keepGoing:"
        << "checking if simulations should continue";

    QStringList                            effectiveNames;
    QMap<QString, TruckSimulationClient *> relevantClients;
    bool shouldContinue = false;

    // First check with read lock
    {
        Commons::ScopedReadLock locker(m_mutex);
        effectiveNames = networkNames.contains("*")
                             ? m_clients.keys()
                             : networkNames;

        // Check if any simulation should continue
        for (const QString &name : effectiveNames)
        {
            if (m_clients.contains(name))
            {
                double progress =
                    m_clients[name]->getProgressPercentage(
                        name);
                if (progress < 100.0)
                {
                    shouldContinue = true;
                    break;
                }

                // Store the client for possible end command
                relevantClients[name] = m_clients[name];
            }
        }
    }

    // If no simulation should continue, end all completed
    // ones
    if (!shouldContinue)
    {
        for (auto it = relevantClients.begin();
             it != relevantClients.end(); ++it)
        {
            it.value()->endSimulator({it.key()});
        }
    }

    return shouldContinue;
}

double TruckSimulationManager::getOverallProgress() const
{
    Commons::ScopedReadLock locker(m_mutex);
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::getOverallProgress:"
        << "clientCount=" << m_clients.size();

    double                  totalProgress = 0.0;
    int                     count         = 0;

    for (auto it = m_clients.begin(); it != m_clients.end();
         ++it)
    {
        totalProgress +=
            it.value()->getProgressPercentage(it.key());
        count++;
    }

    double progress = count ? totalProgress / count : 0.0;
    emit progressUpdated(progress);
    return progress;
}

bool TruckSimulationManager::isConnected() const
{
    Commons::ScopedReadLock locker(m_mutex);
    bool                    result = true;

    for (const auto *client : m_clients.values())
    {
        if (client)
        {
            result *= (client->isConnected());
        }
    }
    return result;
}

bool TruckSimulationManager::hasCommandQueueConsumers()
    const
{
    Commons::ScopedReadLock locker(m_mutex);
    bool                    result = true;

    for (const auto *client : m_clients.values())
    {
        if (client && client->isConnected())
        {
            auto *handler = client->getRabbitMQHandler();
            result *=
                (handler
                 && handler->hasCommandQueueConsumers());
        }
    }
    return result;
}

RabbitMQHandler *
TruckSimulationManager::getRabbitMQHandler() const
{
    Commons::ScopedReadLock locker(m_mutex);
    for (const auto *client : m_clients.values())
    {
        if (client && client->isConnected())
        {
            return client->getRabbitMQHandler();
        }
    }
    return nullptr;
}

TruckSimulationClient *TruckSimulationManager::getClient(
    const QString &networkName)
{
    Commons::ScopedReadLock locker(m_mutex);
    return m_clients.value(networkName, nullptr);
}

QThread *TruckSimulationManager::createClientThread(
    const QString &networkName)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::createClientThread:"
        << "networkName=" << networkName;

    QThread *thread = new QThread();
    thread->setObjectName(
        QString("TruckClient_%1").arg(networkName));

    // Connect thread finished signal to delete later
    connect(thread, &QThread::finished, thread,
            &QThread::deleteLater);

    return thread;
}

void TruckSimulationManager::initializeClientInThread(
    TruckSimulationClient *client,
    const QString         &networkName)
{
    qCDebug(lcClientTruck)
        << "TruckSimulationManager::"
           "initializeClientInThread:"
        << "networkName=" << networkName;

    if (!client)
    {
        qCWarning(lcClientTruck)
            << "TruckSimulationManager::"
               "initializeClientInThread:"
            << "null client pointer"
            << "networkName=" << networkName;
        return;
    }

    // Initialize client in its thread (will be executed
    // when thread starts)
    client->initializeClient(m_defaultSimulationTime,
                             m_defaultTerminalClient,
                             m_defaultLogger);

    // Connect to server (will be executed when thread
    // starts)
    client->connectToServer();

    // Log client initialization
    if (m_defaultLogger)
    {
        m_defaultLogger->log(
            QString("TruckSimulationClient '%1' "
                    "initialized and moved to thread")
                .arg(networkName),
            static_cast<int>(ClientType::TruckClient));
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
