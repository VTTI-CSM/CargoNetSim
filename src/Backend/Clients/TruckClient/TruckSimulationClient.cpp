/**
 * @file TruckSimulationClient.cpp
 * @brief Implements truck simulation client
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#include "TruckSimulationClient.h"
#include "Backend/Commons/LoggerInterface.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QJsonDocument>

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

TruckSimulationClient::TruckSimulationClient(
    const QString &exePath, QObject *parent,
    const QString &host, int port)
    : SimulationClientBase(
          parent, host, port, "CargoNetSim.Exchange",
          "CargoNetSim.CommandQueue.TruckNetSim",
          "CargoNetSim.ResponseQueue.TruckNetSim",
          "CargoNetSim.Command.TruckNetSim",
          QStringList{"CargoNetSim.Response.TruckNetSim"},
          ClientType::TruckClient)
    , m_exePath(exePath)
    , m_tripIdCounter(10000)
    , m_lastRequestId(-1)
    , m_sentMsgCounter(0)
    , m_networkGraph(nullptr)
{
}

TruckSimulationClient::~TruckSimulationClient()
{
    Commons::ScopedWriteLock locker(m_dataMutex);

    // Terminate and clean up all processes
    for (auto *process : m_processes.values())
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->terminate();
            process->waitForFinished(3000);
            if (process->state() != QProcess::NotRunning)
            {
                process->kill();
            }
        }
        delete process;
    }

    // Clean up truck states
    for (auto &stateList : m_truckStates)
    {
        qDeleteAll(stateList);
    }

    m_processes.clear();
    m_truckStates.clear();
}

void TruckSimulationClient::initializeClient(
    SimulationTime           *simulationTime,
    TerminalSimulationClient *terminalClient,
    LoggerInterface          *logger)
{
    // Call base class initialization first
    SimulationClientBase::initializeClient(
        simulationTime, terminalClient, logger);

    // Create managers as children of this client
    m_tripEndCallbackManager =
        new TripEndCallbackManager(this);
    m_asyncTripManager = new AsyncTripManager(this);
    m_containerManager = new ContainerManager(this);

    // Connect signals between components
    connect(this, &TruckSimulationClient::tripEndedWithData,
            m_tripEndCallbackManager,
            &TripEndCallbackManager::onTripEnded);

    connect(m_tripEndCallbackManager,
            &TripEndCallbackManager::tripEnded, this,
            [this](const TripEndData &data) {
                m_asyncTripManager->onTripEnded(
                    data.networkName, data.tripId,
                    data.rawData);
            });

    if (m_logger)
    {
        m_logger->log("TruckSimulationClient initialized",
                      static_cast<int>(m_clientType));
    }
}

bool TruckSimulationClient::defineSimulator(
    const QString &networkName,
    const QString &masterFilePath, double simTime,
    const QMap<QString, QVariant> &configUpdates,
    const QStringList             &argsUpdates)
{
    // Prepare standard command-line arguments
    QStringList args = {
        "--mode",     "controlled",
        "--sim_time", QString::number(simTime),
        "--master",   QFileInfo(masterFilePath).fileName()};

    // Add any additional arguments
    args << argsUpdates;

    // Extract and add configuration updates if provided
    QJsonObject config =
        QJsonObject::fromVariantMap(configUpdates);
    if (!config.isEmpty())
    {
        QString host =
            config.value("MQ_HOST").toString("localhost");
        QString port =
            config.value("MQ_PORT").toString("5672");
        args << "--amq_server" << host << "--amq_port"
             << port;
    }

    // Launch the simulator
    bool success = launchSimulator(
        networkName, masterFilePath, simTime, args);

    if (success)
    {
        Commons::ScopedWriteLock locker(m_dataMutex);
        m_totalSimTimes[networkName] = simTime;
    }

    return success;
}

bool TruckSimulationClient::runSimulator(
    const QStringList &networkNames)
{
    Commons::ScopedReadLock locker(m_dataMutex);
    bool                    allSucceeded = true;

    for (const QString &name : networkNames)
    {
        if (m_processes.contains(name)
            && m_simulationTimes[name]
                   < m_simulationHorizons[name])
        {

            // Format sync message using the new formatter
            QString msg = MessageFormatter::formatSyncGo(
                m_lastRequestId, m_simulationTimes[name],
                m_simulationHorizons[name]);

            // Send command
            bool sent =
                sendCommand(msg.toUtf8(), QJsonObject(),
                            m_sendingRoutingKey);

            if (!sent)
            {
                allSucceeded = false;
            }
        }
    }

    return allSucceeded;
}

bool TruckSimulationClient::advanceByTimeStep(
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

        bool success = sendCommandAndWait(
            "runSimulator",
            params,
            {"simulationadvanced", "simulationended"});

        return success;
    });
}

void TruckSimulationClient::notifyTerminalClosure(
    const QString& terminalId,
    const QString& alternativeId)
{
    // Truck simulator handles this via trip replanning
    qDebug() << "Terminal closure notification:" << terminalId
             << "->" << alternativeId;
}

void TruckSimulationClient::notifyTerminalReopened(
    const QString& terminalId)
{
    qDebug() << "Terminal reopened:" << terminalId;
}

bool TruckSimulationClient::endSimulator(
    const QStringList &networkNames)
{
    Commons::ScopedWriteLock locker(m_dataMutex);
    bool                     allSucceeded = true;

    for (const QString &name : networkNames)
    {
        if (m_processes.contains(name))
        {
            // Format end message using the new formatter
            QString msg = MessageFormatter::formatSyncEnd(
                m_lastRequestId, m_simulationTimes[name]);

            // Send command
            bool sent =
                sendCommand(msg.toUtf8(), QJsonObject(),
                            m_sendingRoutingKey);

            if (!sent)
            {
                allSucceeded = false;
            }

            // Terminate process
            m_processes[name]->terminate();
        }
    }

    return allSucceeded;
}

QString TruckSimulationClient::addTrip(
    const QString &networkName, const QString &originId,
    const QString                           &destinationId,
    const QList<ContainerCore::Container *> &containers)
{
    // Generate a trip ID
    int     tripId    = m_tripIdCounter++;
    QString tripIdStr = QString::number(tripId);

    // Find route links
    QList<int> linkIds;
    if (m_networkGraph)
    {
        QVector<QString> nodes =
            m_networkGraph->findShortestPath(originId,
                                             destinationId);

        linkIds =
            m_networkGraph->convertNodePathToLinkPath(nodes)
                .toList();
    }
    else
    {
        // Fallback to default link IDs if no graph is
        // available
        linkIds = {1, 2, 3};
    }

    // Get current simulation time as start time
    double startTime =
        m_simulationHorizons.value(networkName, 0.0);

    // Create add trip message
    QString msg = MessageFormatter::formatAddTrip(
        m_sentMsgCounter++, tripId, originId.toInt(),
        destinationId.toInt(), startTime, linkIds);

    // Send the command
    bool sent = sendCommand(msg.toUtf8(), QJsonObject(),
                            m_sendingRoutingKey);

    if (!sent)
    {
        return QString();
    }

    Commons::ScopedWriteLock locker(m_dataMutex);

    // Create new truck state
    auto *state = new TruckState(
        networkName, tripId, originId, destinationId, this);

    // Add to tracking
    m_truckStates[networkName].append(state);

    // Assign containers to the trip
    if (!containers.isEmpty())
    {
        m_containerManager->assignContainersToVehicle(
            QString("Truck_%1").arg(tripIdStr), containers);
    }

    return tripIdStr;
}

QFuture<TripResult> TruckSimulationClient::addTripAsync(
    const QString &networkName, const QString &originId,
    const QString                           &destinationId,
    const QList<ContainerCore::Container *> &containers)
{
    // Create trip request
    TripRequest request;
    request.networkName   = networkName;
    request.originId      = originId.toInt();
    request.destinationId = destinationId.toInt();
    request.containers    = containers;

    // Start async operation
    QFuture<TripResult> future =
        m_asyncTripManager->addTripAsync(request);

    // Add trip synchronously
    QString tripId = addTrip(networkName, originId,
                             destinationId, containers);

    // Register the trip with the async manager
    if (!tripId.isEmpty())
    {
        m_asyncTripManager->registerTrip(tripId, request);
    }

    return future;
}

const TruckState *TruckSimulationClient::getTruckState(
    const QString &networkName, const QString &tripId) const
{
    Commons::ScopedReadLock locker(m_dataMutex);

    // Look up truck state
    for (const auto *state :
         m_truckStates.value(networkName))
    {
        if (state->tripId() == tripId)
        {
            return state;
        }
    }

    return nullptr;
}

QList<const TruckState *>
TruckSimulationClient::getAllNetworkTrucksStates(
    const QString &networkName) const
{
    Commons::ScopedReadLock   locker(m_dataMutex);
    QList<const TruckState *> constStates;

    // Convert non-const states to const states
    for (auto *state : m_truckStates.value(networkName))
    {
        constStates.append(state);
    }

    return constStates;
}

double TruckSimulationClient::getProgressPercentage(
    const QString &networkName) const
{
    Commons::ScopedReadLock locker(m_dataMutex);

    // Calculate current progress
    double time = m_simulationTimes.value(networkName, 0.0);
    double total = m_totalSimTimes.value(networkName, 1.0);

    return (time / total) * 100.0;
}

double TruckSimulationClient::getSimulationTime(
    const QString &networkName) const
{
    Commons::ScopedReadLock locker(m_dataMutex);
    return m_simulationTimes.value(networkName, 0.0);
}

void TruckSimulationClient::setNetworkGraph(
    const TransportationGraph<QString> *graph)
{
    m_networkGraph = graph;
}

void TruckSimulationClient::registerTripEndCallback(
    const QString                           &callbackId,
    std::function<void(const TripEndData &)> callback)
{
    m_tripEndCallbackManager->registerGlobalCallback(
        callbackId, callback);
}

void TruckSimulationClient::registerTripSpecificCallback(
    const QString &tripId, const QString &callbackId,
    std::function<void(const TripEndData &)> callback)
{
    m_tripEndCallbackManager->registerTripCallback(
        tripId, callbackId, callback);
}

void TruckSimulationClient::unregisterTripEndCallback(
    const QString &callbackId)
{
    m_tripEndCallbackManager->unregisterGlobalCallback(
        callbackId);
}

ContainerManager *
TruckSimulationClient::getContainerManager() const
{
    return m_containerManager;
}

void TruckSimulationClient::processMessage(
    const QJsonObject &message)
{
    // Delegate for the base class for the initial
    // processing
    SimulationClientBase::processMessage(message);

    // Basic validation
    if (!message.contains("body"))
    {
        return;
    }

    QString     body  = message["body"].toString();
    QStringList parts = body.split('/');

    if (parts.size() < 9)
    {
        return;
    }

    // Parse message components
    int     msgType     = parts[2].toInt();
    int     msgCode     = parts[3].toInt();
    QString networkName = message["networkName"].toString();

    // Handle sync request within a limited scope
    if (msgType
            == static_cast<int>(
                MessageFormatter::MessageType::SYNC)
        && msgCode
               == static_cast<int>(
                   MessageFormatter::MessageCode::SYNC_REQ))
    {
        // First, update data with the lock held
        {
            Commons::ScopedWriteLock locker(m_dataMutex);
            m_simulationTimes[networkName] =
                parts[8].toDouble();
            m_simulationHorizons[networkName] =
                parts[9].toDouble();
            m_lastRequestId = parts[0].toInt();
        }

        // Then, run simulator without holding the lock
        runSimulator({networkName});
        return;
    }

    // Handle trip info type messages
    if (msgType
        == static_cast<int>(
            MessageFormatter::MessageType::TRIPS_INFO))
    {
        // Parse payload as JSON
        QJsonObject payload =
            QJsonDocument::fromJson(parts[8].toUtf8())
                .object();

        if (payload.isEmpty())
        {
            return;
        }

        QString tripId = payload["Trip_ID"].toString();

        // Handle trip end event
        if (msgCode
            == static_cast<int>(
                MessageFormatter::MessageCode::TRIP_END))
        {
            TruckState *state = nullptr;
            TripEndData tripData;

            // First, update state with the lock held and
            // gather needed data
            {
                Commons::ScopedWriteLock locker(
                    m_dataMutex);
                state = const_cast<TruckState *>(
                    getTruckState(networkName, tripId));

                if (state)
                {
                    // Update state
                    state->updateFromJson(payload);

                    // Create trip end data for later
                    // emission
                    tripData.tripId      = tripId;
                    tripData.networkName = networkName;
                    tripData.origin =
                        payload["Origin"].toString();
                    tripData.destination =
                        payload["Destination"].toString();
                    tripData.distance =
                        payload["Trip_Distance"].toDouble();
                    tripData.fuelConsumption =
                        payload["Fuel_Consumption"]
                            .toDouble();
                    tripData.travelTime =
                        payload["Travel_Time"].toDouble();
                    tripData.rawData = payload;
                }
            }

            // Emit signals outside the lock scope if we
            // found a valid state
            if (state)
            {
                emit tripEnded(networkName, tripId);
                emit tripEndedWithData(tripData);
            }

            return;
        }
        // Handle trip info update
        else if (msgCode
                 == static_cast<int>(
                     MessageFormatter::MessageCode::
                         TRIP_INFO))
        {
            Commons::ScopedWriteLock locker(m_dataMutex);
            auto *state = const_cast<TruckState *>(
                getTruckState(networkName, tripId));

            if (state)
            {
                // Update state with info
                state->updateInfoFromJson(payload);
            }

            return;
        }
    }
}

bool TruckSimulationClient::launchSimulator(
    const QString &networkName,
    const QString &masterFilePath, double simTime,
    const QStringList &args)
{
    // Get directory and file information
    QDir    dir(QFileInfo(masterFilePath).absolutePath());
    QString newExePath =
        dir.filePath(QFileInfo(m_exePath).fileName());

    // Copy executable to working directory if needed
    if (!QFile::exists(newExePath))
    {
        if (!QFile::copy(m_exePath, newExePath))
        {
            if (m_logger)
            {
                m_logger->logError(
                    "Failed to copy executable to working "
                    "directory",
                    static_cast<int>(m_clientType));
            }
            return false;
        }

        // Set executable permissions
        QFile::setPermissions(
            newExePath, QFile::ExeUser | QFile::ReadUser
                            | QFile::WriteUser);
    }

    // Create process
    auto *process = new QProcess(parent());
    process->setWorkingDirectory(dir.path());

    // Start the process
    process->start(newExePath, args);
    if (!process->waitForStarted(5000))
    {
        if (m_logger)
        {
            m_logger->logError(
                "Failed to start simulator process",
                static_cast<int>(m_clientType));
        }
        delete process;
        return false;
    }

    Commons::ScopedWriteLock locker(m_dataMutex);
    m_processes[networkName] = process;

    return true;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
