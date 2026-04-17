/**
 * @file CargoNetSimController.cpp
 * @brief Implements multi-modal simulation controller
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "CargoNetSimController.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Utils/Utils.h"
#include <QCoreApplication>
#include <QThread>

namespace CargoNetSim
{

// Initialize static members
CargoNetSimController *CargoNetSimController::s_instance = nullptr;

CargoNetSimController::CargoNetSimController(
    Backend::LoggerInterface *logger, QObject *parent)
    : QObject(parent)
    , m_truckThread(nullptr)
    , m_shipThread(nullptr)
    , m_trainThread(nullptr)
    , m_terminalThread(nullptr)
    , m_truckManager(nullptr)
    , m_shipClient(nullptr)
    , m_trainClient(nullptr)
    , m_terminalClient(nullptr)
    , m_initializedClientCount(0)
    , m_readyClientCount(0)
    , m_logger(logger)
{
    // Release-build hard checks: these invariants hold the whole
    // lifetime model together. Q_ASSERT_X would silently compile
    // out in release, masking double-construction or off-thread
    // construction until much later, far from the call site.
    if (s_instance != nullptr)
    {
        qFatal("CargoNetSimController: attempted to construct a "
               "second instance. Only one may live at a time.");
    }
    if (QCoreApplication::instance()
        && QThread::currentThread()
               != QCoreApplication::instance()->thread())
    {
        qFatal("CargoNetSimController: must be constructed on "
               "the main (QCoreApplication) thread.");
    }
    s_instance = this;

    qCInfo(lcController) << "CargoNetSimController: initializing";

    // Create the NetworkController first
    m_networkController =
        new Backend::NetworkController(this);

    // Then create the RegionDataController with the
    // NetworkController
    m_regionDataController =
        new Backend::RegionDataController(
            m_networkController, this);

    // Create other controllers as needed
    m_vehicleController =
        new Backend::VehicleController(this);

    m_configController = new Backend::ConfigController(
        Backend::Utils::findConfigFilePath(), this);

    // Initialize client status tracking
    m_clientInitialized[Backend::ClientType::TruckClient] =
        false;
    m_clientInitialized[Backend::ClientType::ShipClient] =
        false;
    m_clientInitialized[Backend::ClientType::TrainClient] =
        false;
    m_clientInitialized
        [Backend::ClientType::TerminalClient] = false;
}

CargoNetSimController &CargoNetSimController::getInstance()
{
    // Release-build hard check. Q_ASSERT_X would silently compile
    // out in release, leaving the next deref as the crash site
    // instead of this call site.
    if (s_instance == nullptr)
    {
        qFatal("CargoNetSimController::getInstance: controller "
               "has not been constructed yet. Construct it in "
               "main() before calling getInstance(). Use "
               "instance() for nullable access during startup "
               "or shutdown windows.");
    }
    return *s_instance;
}

CargoNetSimController *CargoNetSimController::instance()
{
    return s_instance;
}

CargoNetSimController::~CargoNetSimController()
{
    // Symmetric thread-affinity check with the constructor. Release-
    // build hard check: destructing off-thread would leave the
    // s_instance pointer in an inconsistent state for any worker
    // thread still reading it.
    if (QCoreApplication::instance()
        && QThread::currentThread()
               != QCoreApplication::instance()->thread())
    {
        qFatal("CargoNetSimController: must be destroyed on "
               "the main (QCoreApplication) thread.");
    }

    qCInfo(lcController) << "CargoNetSimController: shutting down";
    stopAll();

    // Clean up threads
    if (m_truckThread)
    {
        m_truckThread->quit();
        m_truckThread->wait(3000);
        delete m_truckThread;
    }

    if (m_shipThread)
    {
        m_shipThread->quit();
        m_shipThread->wait(3000);
        delete m_shipThread;
    }

    if (m_trainThread)
    {
        m_trainThread->quit();
        m_trainThread->wait(3000);
        delete m_trainThread;
    }

    if (m_terminalThread)
    {
        m_terminalThread->quit();
        m_terminalThread->wait(3000);
        delete m_terminalThread;
    }

    // Clean up controllers
    // The Controllers will be deleted automatically
    // as a child of this object No need for explicit
    // deletion or cleanup classes

    s_instance = nullptr;
}

bool CargoNetSimController::initialize(
    const QString &truckExePath)
{
    qCInfo(lcController) << "CargoNetSimController::initialize:"
                         << "truckExePath=" << truckExePath;
    // Create and start client threads
    bool success = true;
    m_simulationTime = new Backend::SimulationTime();
    qCDebug(lcController) << "CargoNetSimController::initialize:"
                         << "starting terminal client thread";
    success &= initializeTerminalClient();
    qCDebug(lcController) << "CargoNetSimController::initialize:"
                         << "starting truck client thread";
    success &= initializeTruckClient(truckExePath);
    qCDebug(lcController) << "CargoNetSimController::initialize:"
                         << "starting ship client thread";
    success &= initializeShipClient();
    qCDebug(lcController) << "CargoNetSimController::initialize:"
                         << "starting train client thread";
    success &= initializeTrainClient();

    return success;
}

bool CargoNetSimController::startAll()
{
    qCInfo(lcController) << "CargoNetSimController::startAll";
    // Start all threads
    if (m_truckThread && !m_truckThread->isRunning())
    {
        m_truckThread->start();
    }

    if (m_shipThread && !m_shipThread->isRunning())
    {
        m_shipThread->start();
    }

    if (m_trainThread && !m_trainThread->isRunning())
    {
        m_trainThread->start();
    }

    if (m_terminalThread && !m_terminalThread->isRunning())
    {
        m_terminalThread->start();
    }

    return true;
}

bool CargoNetSimController::stopAll()
{
    qCInfo(lcController) << "CargoNetSimController::stopAll";
    // Stop all simulation clients
    if (m_truckManager)
    {
        // Stop all truck instances
        QStringList networks = {"*"};
        // TODO: Terminate all manager clients if running
        // m_truckManager->terminateSimulators(networks);
    }

    if (m_shipClient)
    {
        // TODO: Terminate simulations if running
        // m_shipClient->terminateSimulations({"*"});
    }

    if (m_trainClient)
    {
        // TODO: Terminate simulations if running
        // m_trainClient->terminateSimulations({"*"});
    }

    return true;
}

Backend::RegionDataController *
CargoNetSimController::getRegionDataController()
{
    return m_regionDataController;
}

Backend::VehicleController *
CargoNetSimController::getVehicleController()
{
    return m_vehicleController;
}

Backend::ConfigController *
CargoNetSimController::getConfigController()
{
    return m_configController;
}

Backend::TruckClient::TruckSimulationManager *
CargoNetSimController::getTruckManager() const
{
    return m_truckManager;
}

Backend::NetworkController *
CargoNetSimController::getNetworkController() const
{
    return m_networkController;
}

Backend::ShipClient::ShipSimulationClient *
CargoNetSimController::getShipClient() const
{
    return m_shipClient;
}

Backend::TrainClient::TrainSimulationClient *
CargoNetSimController::getTrainClient() const
{
    return m_trainClient;
}

Backend::TerminalSimulationClient *
CargoNetSimController::getTerminalClient() const
{
    return m_terminalClient;
}

// Implementation of service methods
double CargoNetSimController::getTerminalCapacity(
    const QString &terminalId)
{
    double result = -1.0;
    emit requestTerminalCapacity(terminalId, result);
    return result;
}

int CargoNetSimController::getTerminalContainerCount(
    const QString &terminalId)
{
    int result = -1;
    emit requestContainerCount(terminalId, result);
    return result;
}

bool CargoNetSimController::addContainersToTerminal(
    const QString &terminalId,
    const QString &containersJson)
{
    bool result = false;
    emit requestAddContainers(terminalId, containersJson,
                              result);
    return result;
}

void CargoNetSimController::onThreadStarted()
{
    QThread *senderThread =
        qobject_cast<QThread *>(sender());

    if (senderThread == m_shipThread)
    {
        if (m_shipClient)
        {
            m_shipClient->initializeClient(m_simulationTime,
                                           m_terminalClient,
                                           m_logger);
            m_shipClient->connectToServer();
        }
    }
    else if (senderThread == m_trainThread)
    {
        if (m_trainClient)
        {
            m_trainClient->initializeClient(
                m_simulationTime, m_terminalClient,
                m_logger);
            m_trainClient->connectToServer();
        }
    }
    else if (senderThread == m_terminalThread)
    {
        if (m_terminalClient)
        {
            m_terminalClient->initializeClient(
                m_simulationTime, nullptr, m_logger);
            m_terminalClient->connectToServer();
        }
    }
    else if (senderThread == m_truckThread)
    {
        if (m_truckManager)
        {
            m_truckManager->initializeManager(
                m_simulationTime, m_terminalClient,
                m_logger);
        }
    }
}

void CargoNetSimController::onThreadFinished()
{
    QThread *senderThread =
        qobject_cast<QThread *>(sender());

    if (senderThread == m_shipThread)
    {
        if (m_shipClient)
        {
            m_shipClient->disconnectFromServer();
        }
    }
    else if (senderThread == m_trainThread)
    {
        if (m_trainClient)
        {
            m_trainClient->disconnectFromServer();
        }
    }
    else if (senderThread == m_terminalThread)
    {
        if (m_terminalClient)
        {
            m_terminalClient->disconnectFromServer();
        }
    }
}

bool CargoNetSimController::initializeTruckClient(
    const QString &exePath)
{
    // Create thread for truck client
    m_truckThread = new QThread();
    m_truckThread->setObjectName("TruckSimulationThread");

    // Create truck manager
    m_truckManager =
        new Backend::TruckClient::TruckSimulationManager(
            nullptr);

    // Move truck manager to truck thread
    m_truckManager->moveToThread(m_truckThread);

    // Connect thread signals
    connect(m_truckThread, &QThread::started, this,
            &CargoNetSimController::onThreadStarted);
    connect(m_truckThread, &QThread::finished, this,
            &CargoNetSimController::onThreadFinished);

    // Flag as initialized
    m_clientInitialized[Backend::ClientType::TruckClient] =
        true;
    m_initializedClientCount++;
    emit clientInitialized(
        Backend::ClientType::TruckClient);

    if (m_initializedClientCount == 4)
    {
        emit allClientsInitialized();
    }

    return true;
}

bool CargoNetSimController::initializeShipClient()
{
    // Create thread for ship client
    m_shipThread = new QThread();
    m_shipThread->setObjectName("ShipSimulationThread");

    // Create ship client
    m_shipClient =
        new Backend::ShipClient::ShipSimulationClient(
            nullptr);
    m_shipClient->moveToThread(m_shipThread);

    // Connect thread signals
    connect(m_shipThread, &QThread::started, this,
            &CargoNetSimController::onThreadStarted);
    connect(m_shipThread, &QThread::finished, this,
            &CargoNetSimController::onThreadFinished);

    // Flag as initialized
    m_clientInitialized[Backend::ClientType::ShipClient] =
        true;
    m_initializedClientCount++;
    emit clientInitialized(Backend::ClientType::ShipClient);

    if (m_initializedClientCount == 4)
    {
        emit allClientsInitialized();
    }

    return true;
}

bool CargoNetSimController::initializeTrainClient()
{
    // Create thread for train client
    m_trainThread = new QThread();
    m_trainThread->setObjectName("TrainSimulationThread");

    // Create train client
    m_trainClient =
        new Backend::TrainClient::TrainSimulationClient(
            nullptr);
    m_trainClient->moveToThread(m_trainThread);

    // Connect thread signals
    connect(m_trainThread, &QThread::started, this,
            &CargoNetSimController::onThreadStarted);
    connect(m_trainThread, &QThread::finished, this,
            &CargoNetSimController::onThreadFinished);

    // Flag as initialized
    m_clientInitialized[Backend::ClientType::TrainClient] =
        true;
    m_initializedClientCount++;
    emit clientInitialized(
        Backend::ClientType::TrainClient);

    if (m_initializedClientCount == 4)
    {
        emit allClientsInitialized();
    }

    return true;
}

bool CargoNetSimController::initializeTerminalClient()
{
    // Create thread for terminal client
    m_terminalThread = new QThread();
    m_terminalThread->setObjectName(
        "TerminalSimulationThread");

    // Create terminal client
    m_terminalClient =
        new Backend::TerminalSimulationClient(nullptr);
    m_terminalClient->moveToThread(m_terminalThread);

    // Connect thread signals
    connect(m_terminalThread, &QThread::started, this,
            &CargoNetSimController::onThreadStarted);
    connect(m_terminalThread, &QThread::finished, this,
            &CargoNetSimController::onThreadFinished);

    // Connect signals to terminal client
    connect(
        this,
        &CargoNetSimController::requestTerminalCapacity,
        m_terminalClient,
        [this](const QString &terminalId, double &result) {
            result = m_terminalClient->getAvailableCapacity(
                terminalId);
        },
        Qt::BlockingQueuedConnection);

    connect(
        this, &CargoNetSimController::requestContainerCount,
        m_terminalClient,
        [this](const QString &terminalId, int &result) {
            result = m_terminalClient->getContainerCount(
                terminalId);
        },
        Qt::BlockingQueuedConnection);

    connect(
        this, &CargoNetSimController::requestAddContainers,
        m_terminalClient,
        [this](const QString &terminalId,
               const QString &containersJson,
               bool          &result) {
            result =
                m_terminalClient->addContainersFromJson(
                    terminalId, containersJson);
        },
        Qt::BlockingQueuedConnection);

    // Flag as initialized
    m_clientInitialized
        [Backend::ClientType::TerminalClient] = true;
    m_initializedClientCount++;
    emit clientInitialized(
        Backend::ClientType::TerminalClient);

    if (m_initializedClientCount == 4)
    {
        emit allClientsInitialized();
    }

    return true;
}

// ==========================================
// Simulation Orchestration Implementation
// ==========================================

bool CargoNetSimController::runSimulation()
{
    if (m_simulationRunning)
    {
        qCWarning(lcController) << "Simulation already running";
        return false;
    }

    m_simulationRunning = true;
    m_simulationPaused = false;

    while (m_simulationRunning && !checkSimulationComplete())
    {
        // Handle pause
        while (m_simulationPaused && m_simulationRunning)
        {
            QThread::msleep(100);
            QCoreApplication::processEvents();
        }

        if (!m_simulationRunning)
            break;

        runSimulationStep();
        QCoreApplication::processEvents();
    }

    m_simulationRunning = false;
    emit simulationCompleted();
    return true;
}

bool CargoNetSimController::runSimulationStep()
{
    if (!m_simulationTime)
    {
        qCCritical(lcController) << "SimulationTime not initialized";
        return false;
    }

    double deltaT = m_simulationTime->getTimeStep();
    double currentTime = m_simulationTime->getCurrentTime();

    // Step 1: Advance all simulators by deltaT
    advanceAllSimulators(deltaT);

    // Step 2: Update System Dynamics for all terminals
    updateAllTerminalsSD(currentTime + deltaT, deltaT);

    // Step 3: Process any events (arrivals, departures)
    processSimulatorEvents();

    // Step 4: Advance simulation time
    m_simulationTime->advanceByTimeStep();

    // Calculate progress
    double progress = 0.0;
    if (m_simulationEndTime > 0.0)
    {
        progress = (m_simulationTime->getCurrentTime() / m_simulationEndTime) * 100.0;
        progress = qMin(progress, 100.0);
    }

    emit simulationStepCompleted(m_simulationTime->getCurrentTime(), progress);

    return !checkSimulationComplete();
}

void CargoNetSimController::pauseSimulation()
{
    if (m_simulationRunning && !m_simulationPaused)
    {
        m_simulationPaused = true;
        emit simulationPaused();
    }
}

void CargoNetSimController::resumeSimulation()
{
    if (m_simulationRunning && m_simulationPaused)
    {
        m_simulationPaused = false;
        emit simulationResumed();
    }
}

void CargoNetSimController::stopSimulation()
{
    m_simulationRunning = false;
    m_simulationPaused = false;
}

bool CargoNetSimController::isSimulationRunning() const
{
    return m_simulationRunning;
}

bool CargoNetSimController::isSimulationPaused() const
{
    return m_simulationPaused;
}

double CargoNetSimController::getCurrentSimulationTime() const
{
    return m_simulationTime ? m_simulationTime->getCurrentTime() : 0.0;
}

void CargoNetSimController::setSimulationEndTime(double endTime)
{
    m_simulationEndTime = endTime;
}

void CargoNetSimController::advanceAllSimulators(double deltaT)
{
    // Advance Ship simulator
    if (m_shipClient && m_shipClient->isConnected())
    {
        m_shipClient->advanceByTimeStep({"*"}, deltaT);
    }

    // Advance Train simulator
    if (m_trainClient && m_trainClient->isConnected())
    {
        m_trainClient->advanceByTimeStep({"*"}, deltaT);
    }

    // Advance Truck simulators (via manager)
    if (m_truckManager)
    {
        QStringList networks = m_truckManager->getAllClientNames();
        for (const QString& network : networks)
        {
            auto* client = m_truckManager->getClient(network);
            if (client && client->isConnected())
            {
                client->advanceByTimeStep({network}, deltaT);
            }
        }
    }
}

void CargoNetSimController::updateAllTerminalsSD(double currentTime, double deltaT)
{
    if (!m_terminalClient || !m_terminalClient->isConnected())
        return;

    // Convert deltaT from seconds to hours for SD (SD uses hours)
    double deltaTHours = deltaT / 3600.0;

    m_terminalClient->updateAllTerminalsSystemDynamics(currentTime, deltaTHours);
}

void CargoNetSimController::processSimulatorEvents()
{
    // This method processes arrivals and departures
    // The actual container movements are handled by the simulators
    // and terminal client through their event handlers

    // Future enhancement: Add explicit event collection and processing here
}

bool CargoNetSimController::checkSimulationComplete()
{
    // Check if simulation has reached end time
    if (m_simulationEndTime > 0.0 &&
        m_simulationTime->getCurrentTime() >= m_simulationEndTime)
    {
        return true;
    }

    // Could also check if all vehicles reached destinations
    return false;
}

// ==========================================
// Dynamic Interventions Implementation
// ==========================================

bool CargoNetSimController::closeTerminal(const QString& terminalId,
                                          const QString& alternativeTerminalId)
{
    if (terminalId.isEmpty() || alternativeTerminalId.isEmpty())
    {
        qCWarning(lcController) << "Invalid terminal IDs for closure";
        return false;
    }

    // Store closure mapping
    m_closedTerminals[terminalId] = alternativeTerminalId;

    // Notify all simulators about the closure
    if (m_shipClient)
    {
        m_shipClient->notifyTerminalClosure(terminalId, alternativeTerminalId);
    }

    if (m_trainClient)
    {
        m_trainClient->notifyTerminalClosure(terminalId, alternativeTerminalId);
    }

    if (m_truckManager)
    {
        for (const QString& network : m_truckManager->getAllClientNames())
        {
            auto* client = m_truckManager->getClient(network);
            if (client)
            {
                client->notifyTerminalClosure(terminalId, alternativeTerminalId);
            }
        }
    }

    emit terminalClosed(terminalId, alternativeTerminalId);
    return true;
}

bool CargoNetSimController::reopenTerminal(const QString& terminalId)
{
    if (!m_closedTerminals.contains(terminalId))
    {
        qCWarning(lcController) << "Terminal not in closed list:" << terminalId;
        return false;
    }

    m_closedTerminals.remove(terminalId);

    // Notify all simulators
    if (m_shipClient)
    {
        m_shipClient->notifyTerminalReopened(terminalId);
    }

    if (m_trainClient)
    {
        m_trainClient->notifyTerminalReopened(terminalId);
    }

    if (m_truckManager)
    {
        for (const QString& network : m_truckManager->getAllClientNames())
        {
            auto* client = m_truckManager->getClient(network);
            if (client)
            {
                client->notifyTerminalReopened(terminalId);
            }
        }
    }

    emit terminalReopened(terminalId);
    return true;
}

} // namespace CargoNetSim
