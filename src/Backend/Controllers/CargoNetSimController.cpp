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
#include <QEventLoop>
#include <QThread>
#include <QTimer>

namespace CargoNetSim
{

namespace
{
constexpr int kControllerThreadStopTimeoutMs = 10000;

template <typename Client>
bool disconnectClientOnOwningThread(Client *client,
                                    const char *clientName)
{
    if (client == nullptr)
        return true;

    auto disconnect = [client, clientName]() -> bool {
        try
        {
            auto *handler = client->getRabbitMQHandler();
            if (handler == nullptr || !handler->isConnected())
                return true;

            client->disconnectFromServer();
            return true;
        }
        catch (const std::exception &e)
        {
            qCWarning(lcController)
                << "CargoNetSimController::stopAll:"
                << clientName
                << "disconnect failed:" << e.what();
        }
        catch (...)
        {
            qCWarning(lcController)
                << "CargoNetSimController::stopAll:"
                << clientName
                << "disconnect failed with unknown exception";
        }
        return false;
    };

    QThread *ownerThread = client->thread();
    if (ownerThread && ownerThread->isRunning()
        && ownerThread != QThread::currentThread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            client, [&]() { result = disconnect(); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            qCWarning(lcController)
                << "CargoNetSimController::stopAll:"
                << "failed to queue disconnect for" << clientName;
            return false;
        }
        return result;
    }

    return disconnect();
}

bool stopControllerThread(QThread *thread, const char *threadName)
{
    if (thread == nullptr || !thread->isRunning())
        return true;

    thread->quit();
    if (thread->wait(kControllerThreadStopTimeoutMs))
        return true;

    qCCritical(lcController)
        << "CargoNetSimController::stopAll:"
        << threadName
        << "did not stop within"
        << kControllerThreadStopTimeoutMs << "ms";
    return false;
}

void deleteStoppedThread(QThread *&thread, const char *threadName)
{
    if (thread == nullptr)
        return;

    if (thread->isRunning())
    {
        qCCritical(lcController)
            << "CargoNetSimController: refusing to delete running"
            << threadName;
        return;
    }

    delete thread;
    thread = nullptr;
}

} // namespace

// Initialize static members
std::atomic<CargoNetSimController *>
    CargoNetSimController::s_instance{nullptr};

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
    , m_completedStartupCount(0)
    , m_readyClientCount(0)
    , m_logger(logger)
{
    // Release-build hard checks: these invariants hold the whole
    // lifetime model together. Q_ASSERT_X would silently compile
    // out in release, masking double-construction or off-thread
    // construction until much later, far from the call site.
    if (QCoreApplication::instance()
        && QThread::currentThread()
               != QCoreApplication::instance()->thread())
    {
        qFatal("CargoNetSimController: must be constructed on "
               "the main (QCoreApplication) thread.");
    }
    // Atomically claim the single-instance slot. compare_exchange
    // both asserts s_instance was nullptr AND publishes 'this' in
    // one operation. Release ordering so downstream reads see a
    // fully-constructed object.
    CargoNetSimController *expected = nullptr;
    if (!s_instance.compare_exchange_strong(
            expected, this, std::memory_order_release,
            std::memory_order_relaxed))
    {
        qFatal("CargoNetSimController: attempted to construct a "
               "second instance. Only one may live at a time.");
    }

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
    // Acquire load: pairs with the release store in the
    // constructor, so downstream accesses see a fully-constructed
    // object.
    CargoNetSimController *p = s_instance.load(std::memory_order_acquire);
    if (p == nullptr)
    {
        qFatal("CargoNetSimController::getInstance: controller "
               "has not been constructed yet. Construct it in "
               "main() before calling getInstance(). Use "
               "instance() for nullable access during startup "
               "or shutdown windows.");
    }
    return *p;
}

CargoNetSimController *CargoNetSimController::instance()
{
    return s_instance.load(std::memory_order_acquire);
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
    deleteStoppedThread(m_truckThread, "TruckSimulationThread");
    deleteStoppedThread(m_shipThread, "ShipSimulationThread");
    deleteStoppedThread(m_trainThread, "TrainSimulationThread");
    deleteStoppedThread(m_terminalThread, "TerminalSimulationThread");

    // Clean up controllers
    // The Controllers will be deleted automatically
    // as a child of this object No need for explicit
    // deletion or cleanup classes

    // Release store: pairs with acquire loads in instance() /
    // getInstance(). Any worker thread still running after the
    // 3s wait timeout will see the nullptr with proper ordering.
    s_instance.store(nullptr, std::memory_order_release);
}

bool CargoNetSimController::initialize(
    const QString &truckExePath)
{
    qCInfo(lcController) << "CargoNetSimController::initialize:"
                         << "truckExePath=" << truckExePath;

    m_truckExecutablePath = truckExePath;
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
    m_completedStartupCount = 0;
    m_readyClientCount      = 0;
    m_clientReady.clear();
    m_clientStartupErrors.clear();
    // Start all threads
    if (m_truckThread && !m_truckThread->isRunning())
    {
        qCInfo(lcController) << "CargoNetSimController::startAll:"
                             << "starting truck thread"
                             << m_truckThread
                             << m_truckThread->objectName();
        m_truckThread->start();
        queueTruckManagerStartup();
    }

    if (m_shipThread && !m_shipThread->isRunning())
    {
        qCInfo(lcController) << "CargoNetSimController::startAll:"
                             << "starting ship thread"
                             << m_shipThread
                             << m_shipThread->objectName();
        m_shipThread->start();
        queueShipClientStartup();
    }

    if (m_trainThread && !m_trainThread->isRunning())
    {
        qCInfo(lcController) << "CargoNetSimController::startAll:"
                             << "starting train thread"
                             << m_trainThread
                             << m_trainThread->objectName();
        m_trainThread->start();
        queueTrainClientStartup();
    }

    if (m_terminalThread && !m_terminalThread->isRunning())
    {
        qCInfo(lcController) << "CargoNetSimController::startAll:"
                             << "starting terminal thread"
                             << m_terminalThread
                             << m_terminalThread->objectName();
        m_terminalThread->start();
        queueTerminalClientStartup();
    }

    return true;
}

bool CargoNetSimController::waitForAllClientsReady(
    int timeoutMs, QString *errorMessage)
{
    static constexpr int kExpectedClients = 4;

    const auto allReady = [&]() {
        return m_readyClientCount == kExpectedClients;
    };
    const auto allCompleted = [&]() {
        return m_completedStartupCount == kExpectedClients;
    };

    if (allReady())
        return true;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(this, &CargoNetSimController::allClientsReady, &loop,
            &QEventLoop::quit);
    connect(this, &CargoNetSimController::clientStartupFinished, &loop,
            [&]() {
                if (allCompleted())
                    loop.quit();
            });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    if (allReady())
        return true;

    if (errorMessage != nullptr)
    {
        if (!allCompleted())
        {
            *errorMessage = QStringLiteral(
                "Timed out waiting for queued client-thread startup to finish");
        }
        else
        {
            QStringList failures;
            for (auto it = m_clientStartupErrors.cbegin();
                 it != m_clientStartupErrors.cend(); ++it)
            {
                if (it.value().isEmpty())
                    continue;

                failures.append(QStringLiteral("%1: %2")
                                    .arg(static_cast<int>(it.key()))
                                    .arg(it.value()));
            }

            *errorMessage =
                failures.isEmpty()
                    ? QStringLiteral(
                          "One or more clients failed to become ready")
                    : failures.join(QStringLiteral("; "));
        }
    }

    return false;
}

bool CargoNetSimController::stopAll()
{
    qCInfo(lcController) << "CargoNetSimController::stopAll";
    bool success = true;

    // Stop all simulation clients
    if (m_truckManager)
    {
        // Stop all truck instances
        QStringList networks = {"*"};
        // TODO: Terminate all manager clients if running
        // m_truckManager->terminateSimulators(networks);
    }

    success &= disconnectClientOnOwningThread(
        m_shipClient, "ShipClient");
    success &= disconnectClientOnOwningThread(
        m_trainClient, "TrainClient");
    success &= disconnectClientOnOwningThread(
        m_terminalClient, "TerminalClient");

    success &= stopControllerThread(
        m_truckThread, "TruckSimulationThread");
    success &= stopControllerThread(
        m_shipThread, "ShipSimulationThread");
    success &= stopControllerThread(
        m_trainThread, "TrainSimulationThread");
    success &= stopControllerThread(
        m_terminalThread, "TerminalSimulationThread");

    return success;
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

QString CargoNetSimController::truckExecutablePath() const
{
    return m_truckExecutablePath;
}

bool CargoNetSimController::loadConfig()
{
    return m_configController
               ? m_configController->loadConfig()
               : false;
}

QVariantMap CargoNetSimController::getAllConfigParams() const
{
    return m_configController
               ? m_configController->getAllParams()
               : QVariantMap{};
}

QVariantMap CargoNetSimController::getSimulationParams() const
{
    return m_configController
               ? m_configController->getSimulationParams()
               : QVariantMap{};
}

QVariantMap CargoNetSimController::getCostFunctionWeights() const
{
    return m_configController
               ? m_configController->getCostFunctionWeights()
               : QVariantMap{};
}

void CargoNetSimController::updateConfig(
    const QVariantMap &newConfig)
{
    if (m_configController)
        m_configController->updateConfig(newConfig);
}

bool CargoNetSimController::saveConfig()
{
    return m_configController
               ? m_configController->saveConfig()
               : false;
}

QVector<Backend::Train *> CargoNetSimController::getAllTrains() const
{
    return m_vehicleController
               ? m_vehicleController->getAllTrains()
               : QVector<Backend::Train *>{};
}

QVector<Backend::Ship *> CargoNetSimController::getAllShips() const
{
    return m_vehicleController
               ? m_vehicleController->getAllShips()
               : QVector<Backend::Ship *>{};
}

bool CargoNetSimController::updateTrains(
    QVector<Backend::Train *> trains)
{
    return m_vehicleController
               ? m_vehicleController->updateTrains(std::move(trains))
               : false;
}

bool CargoNetSimController::updateShips(
    QVector<Backend::Ship *> ships)
{
    return m_vehicleController
               ? m_vehicleController->updateShips(std::move(ships))
               : false;
}

bool CargoNetSimController::hasAnyTrains() const
{
    return m_vehicleController
               && !m_vehicleController->getAllTrains().isEmpty();
}

bool CargoNetSimController::hasAnyShips() const
{
    return m_vehicleController
               && !m_vehicleController->getAllShips().isEmpty();
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

void CargoNetSimController::queueTruckManagerStartup()
{
    if (!m_truckManager)
        return;

    const bool invoked = QMetaObject::invokeMethod(
        m_truckManager,
        [this]() {
            bool success        = false;
            QString errorMessage;
            qCDebug(lcController)
                << "CargoNetSimController::queueTruckManagerStartup:"
                << "currentThread=" << QThread::currentThread()
                << "managerThread=" << m_truckManager->thread();
            try
            {
                m_truckManager->initializeManager(
                    m_simulationTime, m_terminalClient, m_logger);
                success = true;
            }
            catch (const std::exception &e)
            {
                errorMessage = QString::fromUtf8(e.what());
            }
            catch (...)
            {
                errorMessage = QStringLiteral(
                    "Unknown truck manager startup failure");
            }

            QMetaObject::invokeMethod(
                this,
                [this, success, errorMessage]() {
                    recordClientStartupResult(
                        Backend::ClientType::TruckClient, success,
                        errorMessage);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);

    if (!invoked)
    {
        qCCritical(lcController)
            << "CargoNetSimController::queueTruckManagerStartup:"
            << "failed to queue truck manager startup";
    }
}

void CargoNetSimController::queueShipClientStartup()
{
    if (!m_shipClient)
        return;

    const bool invoked = QMetaObject::invokeMethod(
        m_shipClient,
        [this]() {
            bool success        = false;
            QString errorMessage;
            qCDebug(lcController)
                << "CargoNetSimController::queueShipClientStartup:"
                << "currentThread=" << QThread::currentThread()
                << "clientThread=" << m_shipClient->thread();
            try
            {
                m_shipClient->initializeClient(
                    m_simulationTime, m_terminalClient, m_logger);
                success = m_shipClient->connectToServer();
                if (!success)
                {
                    errorMessage = QStringLiteral(
                        "Ship client failed to connect to RabbitMQ");
                }
            }
            catch (const std::exception &e)
            {
                errorMessage = QString::fromUtf8(e.what());
            }
            catch (...)
            {
                errorMessage = QStringLiteral(
                    "Unknown ship client startup failure");
            }

            QMetaObject::invokeMethod(
                this,
                [this, success, errorMessage]() {
                    recordClientStartupResult(
                        Backend::ClientType::ShipClient, success,
                        errorMessage);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);

    if (!invoked)
    {
        qCCritical(lcController)
            << "CargoNetSimController::queueShipClientStartup:"
            << "failed to queue ship client startup";
    }
}

void CargoNetSimController::queueTrainClientStartup()
{
    if (!m_trainClient)
        return;

    const bool invoked = QMetaObject::invokeMethod(
        m_trainClient,
        [this]() {
            bool success        = false;
            QString errorMessage;
            qCDebug(lcController)
                << "CargoNetSimController::queueTrainClientStartup:"
                << "currentThread=" << QThread::currentThread()
                << "clientThread=" << m_trainClient->thread();
            try
            {
                m_trainClient->initializeClient(
                    m_simulationTime, m_terminalClient, m_logger);
                success = m_trainClient->connectToServer();
                if (!success)
                {
                    errorMessage = QStringLiteral(
                        "Train client failed to connect to RabbitMQ");
                }
            }
            catch (const std::exception &e)
            {
                errorMessage = QString::fromUtf8(e.what());
            }
            catch (...)
            {
                errorMessage = QStringLiteral(
                    "Unknown train client startup failure");
            }

            QMetaObject::invokeMethod(
                this,
                [this, success, errorMessage]() {
                    recordClientStartupResult(
                        Backend::ClientType::TrainClient, success,
                        errorMessage);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);

    if (!invoked)
    {
        qCCritical(lcController)
            << "CargoNetSimController::queueTrainClientStartup:"
            << "failed to queue train client startup";
    }
}

void CargoNetSimController::queueTerminalClientStartup()
{
    if (!m_terminalClient)
        return;

    const bool invoked = QMetaObject::invokeMethod(
        m_terminalClient,
        [this]() {
            bool success        = false;
            QString errorMessage;
            qCDebug(lcController)
                << "CargoNetSimController::queueTerminalClientStartup:"
                << "currentThread=" << QThread::currentThread()
                << "clientThread=" << m_terminalClient->thread();
            try
            {
                m_terminalClient->initializeClient(
                    m_simulationTime, nullptr, m_logger);
                success = m_terminalClient->connectToServer();
                if (!success)
                {
                    errorMessage = QStringLiteral(
                        "Terminal client failed to connect to RabbitMQ");
                }
            }
            catch (const std::exception &e)
            {
                errorMessage = QString::fromUtf8(e.what());
            }
            catch (...)
            {
                errorMessage = QStringLiteral(
                    "Unknown terminal client startup failure");
            }

            QMetaObject::invokeMethod(
                this,
                [this, success, errorMessage]() {
                    recordClientStartupResult(
                        Backend::ClientType::TerminalClient, success,
                        errorMessage);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);

    if (!invoked)
    {
        qCCritical(lcController)
            << "CargoNetSimController::queueTerminalClientStartup:"
            << "failed to queue terminal client startup";
    }
}

void CargoNetSimController::recordClientStartupResult(
    Backend::ClientType clientType, bool success,
    const QString &errorMessage)
{
    m_completedStartupCount++;
    m_clientReady[clientType] = success;

    if (success)
    {
        m_readyClientCount++;
        emit clientReady(clientType);
    }
    else
    {
        m_clientStartupErrors[clientType] = errorMessage;
        qCCritical(lcController)
            << "CargoNetSimController: client startup failed"
            << static_cast<int>(clientType) << errorMessage;
    }

    emit clientStartupFinished(clientType, success, errorMessage);

    if (m_readyClientCount == 4)
    {
        emit allClientsReady();
    }
}

void CargoNetSimController::onThreadFinished()
{
    QThread *senderThread =
        qobject_cast<QThread *>(sender());

    qCDebug(lcController)
        << "CargoNetSimController::onThreadFinished:"
        << senderThread;
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
