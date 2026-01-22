#include "SimulationClientBase.h"
#include <QDateTime>
#include <QDebug>
#include <QDomDocument>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>
#include <QUuid>
#ifdef HAVE_QTKEYCHAIN
#include <qt6keychain/keychain.h>
#endif

#include "Backend/Models/SimulationTime.h"
#include "Backend/Utils/Utils.h"
#include "QtWidgets/qmessagebox.h"

namespace CargoNetSim
{
namespace Backend
{

/**
 * Constructor initializes the client with connection
 * parameters and sets up the RabbitMQ handler.
 */
SimulationClientBase::SimulationClientBase(
    QObject *parent, const QString &host, int port,
    const QString &exchange, const QString &commandQueue,
    const QString     &responseQueue,
    const QString     &sendingRoutingKey,
    const QStringList &receivingRoutingKeys,
    ClientType         clientType)
    : QObject(parent)
    , m_clientType(clientType)
    , m_host(host)
    , m_port(port)
    , m_exchange(exchange)
    , m_commandQueue(commandQueue)
    , m_responseQueue(responseQueue)
    , m_sendingRoutingKey(sendingRoutingKey.isEmpty()
                              ? "default_key"
                              : sendingRoutingKey)
    , m_receivingRoutingKeys(
          receivingRoutingKeys.isEmpty()
              ? QStringList{"default_key"}
              : receivingRoutingKeys)
    , m_processingCommand(false)
{
}

/**
 * Destructor ensures clean shutdown.
 */
SimulationClientBase::~SimulationClientBase()
{
    disconnectFromServer();

    // Delete the rabbitMQHandler
    if (m_rabbitMQHandler)
    {
        delete m_rabbitMQHandler;
    }

    if (m_logger)
    {
        delete m_logger;
    }
    qDebug() << "SimulationClientBase destroyed for"
             << getClientTypeString();
}

void SimulationClientBase::initializeClient(
    SimulationTime           *simulationTime,
    TerminalSimulationClient *terminalClient,
    LoggerInterface          *logger)
{

    // Set the SimulationTime and logger interface
    m_logger = logger;
    m_simulationTime = simulationTime;
    m_terminalClient = terminalClient;

    // Load RabbitMQ configuration from file and keychain
    loadRabbitMQConfig();

    // Create RabbitMQ handler
    m_rabbitMQHandler = new RabbitMQHandler(
        nullptr, m_host, m_port, m_username, m_password,
        m_exchange, m_commandQueue, m_responseQueue,
        m_sendingRoutingKey, m_receivingRoutingKeys);

    // Connect signals and slots
    connect(m_rabbitMQHandler,
            &RabbitMQHandler::messageReceived, this,
            &SimulationClientBase::handleMessage,
            Qt::QueuedConnection);

    connect(m_rabbitMQHandler,
            &RabbitMQHandler::connectionChanged, this,
            &SimulationClientBase::connectionStatusChanged,
            Qt::QueuedConnection);

    connect(m_rabbitMQHandler,
            &RabbitMQHandler::errorOccurred, this,
            &SimulationClientBase::errorOccurred,
            Qt::QueuedConnection);

    qDebug() << "SimulationClientBase initialized for"
             << getClientTypeString();
    if (m_logger)
    {
        m_logger->log(
            "SimulationClientBase initialized for "
                + getClientTypeString(),
            static_cast<int>(m_clientType));
    }
}

void SimulationClientBase::setController(
    CargoNetSimController *controller)
{
    m_controller = controller;
}

/**
 * Checks if the client is connected to the server.
 */
bool SimulationClientBase::isConnected() const
{
    return m_rabbitMQHandler
           && m_rabbitMQHandler->isConnected();
}

/**
 * Connects to the simulation server.
 */
bool SimulationClientBase::connectToServer()
{
    // Check if m_rabbitMQHandler exists
    if (m_rabbitMQHandler == nullptr)
    {
        qWarning() << "Cannot execute command: RabbitMQ "
                      "handler not initialized";
        if (m_logger)
        {
            m_logger->logError(
                "Cannot execute command: RabbitMQ "
                "handler not initialized",
                static_cast<int>(m_clientType));
        }
        throw std::runtime_error("Client not ready for"
                                 " command execution");
    }

    bool success = m_rabbitMQHandler->establishConnection();

    if (success)
    {
        qDebug() << getClientTypeString()
                 << "connected to server";
    }
    else
    {
        qWarning() << getClientTypeString()
                   << "failed to connect to server";
        if (m_logger)
        {
            m_logger->logError(
                getClientTypeString()
                    + " failed to connect to server",
                static_cast<int>(m_clientType));

            QMessageBox msgBox(nullptr);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setWindowTitle("RabbitMQ Connection Failed");
            msgBox.setText("Could not connect to RabbitMQ server");
            msgBox.setInformativeText(
                "The application failed to establish a connection to the RabbitMQ message broker. "
                "This means that simulation features will not be available.\n\n"
                "Please check:\n"
                "• RabbitMQ server is running\n"
                "• Network connectivity\n"
                "• Firewall settings\n"
                "• Server configuration\n\n"
                "The application will terminate now."
                );
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Retry);
            msgBox.setDefaultButton(QMessageBox::Ok);
        }
    }

    return success;
}

/**
 * Disconnects from the simulation server.
 */
void SimulationClientBase::disconnectFromServer()
{
    // Check if m_rabbitMQHandler exists
    if (m_rabbitMQHandler == nullptr)
    {
        qWarning() << "Cannot execute command: RabbitMQ "
                      "handler not initialized";
        if (m_logger)
        {
            m_logger->logError(
                "Cannot execute command: RabbitMQ "
                "handler not initialized",
                static_cast<int>(m_clientType));
        }
        throw std::runtime_error("Client not ready for"
                                 " command execution");
    }

    m_rabbitMQHandler->stopHeartbeat();
    m_rabbitMQHandler->disconnect();
    qDebug() << getClientTypeString()
             << "disconnected from server";
}

/**
 * Get client type enumeration
 */
ClientType SimulationClientBase::getClientType() const
{
    return m_clientType;
}

/**
 * Get client type as string
 */
QString SimulationClientBase::getClientTypeString() const
{
    switch (m_clientType)
    {
    case ClientType::ShipClient:
        return "ShipClient";
    case ClientType::TrainClient:
        return "TrainClient";
    case ClientType::TruckClient:
        return "TruckClient";
    case ClientType::TerminalClient:
        return "TerminalClient";
    default:
        return "UnknownClient";
    }
}

/**
 * Sends a command and waits for specific response events
 */
bool SimulationClientBase::sendCommandAndWait(
    const QString &command, const QJsonObject &params,
    const QStringList &expectedEvents, int timeoutMs,
    const QString &routingKey)
{
    // Early check to avoid unnecessary work
    if (expectedEvents.isEmpty())
    {
        qWarning()
            << "Cannot wait for empty expected events list";
        if (m_logger)
        {
            m_logger->logError(
                "Cannot wait for empty expected events "
                "list",
                static_cast<int>(m_clientType));
        }
        return false;
    }

    // Clear any previously received events with the same
    // names
    {
        CargoNetSim::Backend::Commons::ScopedWriteLock
            locker(m_eventMutex);
        for (const QString &event : expectedEvents)
        {
            QString normalized = normalizeEventName(event);
            m_receivedEvents.remove(normalized);
        }
    }

    // Send the command
    bool sent = sendCommand(command, params, routingKey);
    if (!sent)
    {
        qWarning() << "Failed to send command:" << command;
        if (m_logger)
        {
            m_logger->logError(
                "Failed to send command: " + command,
                static_cast<int>(m_clientType));
        }
        // Clear any events that may have been registered
        return false;
    }

    // Wait for the expected event
    bool received = waitForEvent(expectedEvents, timeoutMs);
    if (!received)
    {
        qWarning()
            << "Timeout waiting for response to command:"
            << command;
        if (m_logger)
        {
            m_logger->logError(
                "Timeout waiting for response to command: "
                    + command,
                static_cast<int>(m_clientType));
        }
        return false;
    }

    return true;
}

/**
 * Sends a command without waiting for a response
 */
bool SimulationClientBase::sendCommand(
    const QString &command, const QJsonObject &params,
    const QString &routingKey, bool sendAsText)
{
    QJsonObject commandObj =
        createCommandObject(command, params);

    // Add command ID for tracking
    QString commandId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    commandObj["commandId"] = commandId;

    qDebug() << "Sending command" << command << "with ID"
             << commandId;

    // Send the command
    bool success = m_rabbitMQHandler->sendCommand(
        commandObj, routingKey);

    if (success)
    {
        emit commandSent(commandId, command);
    }
    else
    {
        QString errorMsg = "Failed to send command";
        emit errorOccurred(errorMsg);
    }

    return success;
}

/**
 * Creates a command object with parameters.
 */
QJsonObject SimulationClientBase::createCommandObject(
    const QString &command, const QJsonObject &params)
{
    QJsonObject commandObj;
    commandObj["command"] = command;
    commandObj["timestamp"] =
        QDateTime::currentDateTime().toString(Qt::ISODate);
    commandObj["clientType"] =
        static_cast<int>(m_clientType);

    // Add parameters if they exist
    if (!params.isEmpty())
    {
        commandObj["params"] = params;
    }

    return commandObj;
}

/**
 * Waits for any of the specified events to be received.
 */
bool SimulationClientBase::waitForEvent(
    const QStringList &expectedEvents, int timeoutMs)
{
    if (expectedEvents.isEmpty())
    {
        qWarning() << "No event to wait for";
        return false;
    }

    QStringList normalizedEvents;
    for (const QString &event : expectedEvents)
    {
        normalizedEvents.append(normalizeEventName(event));
    }

    QElapsedTimer timer;
    timer.start();
    bool receivedEvent = false;

    while (!receivedEvent)
    {
        // First check with a read lock if event is already
        // received
        {
            CargoNetSim::Backend::Commons::ScopedReadLock
                readLocker(m_eventMutex);
            for (const QString &event : normalizedEvents)
            {
                if (m_receivedEvents.contains(event))
                {
                    if (m_logger)
                    {
                        m_logger->log(
                            "Event " + event + " received",
                            static_cast<int>(m_clientType));
                    }
                    else
                    {
                        qDebug() << "Event" << event
                                 << "received";
                    }
                    receivedEvent = true;
                    break;
                }
            }
        }

        // If event not received, wait with a write lock
        if (!receivedEvent)
        {
            CargoNetSim::Backend::Commons::ScopedWriteLock
                writeLocker(m_eventMutex);

            // Check again after acquiring write lock
            for (const QString &event : normalizedEvents)
            {
                if (m_receivedEvents.contains(event))
                {
                    receivedEvent = true;
                    break;
                }
            }

            // If still not received, wait
            if (!receivedEvent)
            {
                if (timeoutMs <= 0)
                {
                    // Wait indefinitely for the event
                    m_eventCondition.wait(&m_eventMutex);
                }
                else
                {
                    // Calculate remaining time to wait
                    int remainingTime =
                        timeoutMs - timer.elapsed();
                    if (remainingTime <= 0)
                    {
                        if (m_logger)
                        {
                            m_logger->logError(
                                "Timeout waiting for "
                                "response to command",
                                static_cast<int>(
                                    m_clientType));
                        }
                        return false;
                    }
                    m_eventCondition.wait(&m_eventMutex,
                                          remainingTime);
                }

                // Check one more time after waiting
                for (const QString &event :
                     normalizedEvents)
                {
                    if (m_receivedEvents.contains(event))
                    {
                        receivedEvent = true;
                        break;
                    }
                }
            }
        }
    }

    return true;
}

/**
 * Checks if a specific event has been received.
 */
bool SimulationClientBase::hasReceivedEvent(
    const QString &eventName) const
{
    CargoNetSim::Backend::Commons::ScopedReadLock locker(
        m_eventMutex);
    QString normalizedEvent = normalizeEventName(eventName);
    return m_receivedEvents.contains(normalizedEvent);
}

/**
 * Gets data for a received event.
 */
QJsonObject SimulationClientBase::getEventData(
    const QString &eventName) const
{
    CargoNetSim::Backend::Commons::ScopedReadLock locker(
        m_eventMutex);
    QString normalizedEvent = normalizeEventName(eventName);
    return m_receivedEvents.value(normalizedEvent);
}

/**
 * Processes a message from the server.
 */
void SimulationClientBase::processMessage(
    const QJsonObject &message)
{
    // Extract event name if present
    QString eventName;
    if (message.contains("event"))
    {
        eventName = message.value("event").toString();
        QString normalizedEvent =
            normalizeEventName(eventName);

        // Register event
        registerEvent(normalizedEvent, message);

        // Emit signal for the event
        emit eventReceived(normalizedEvent, message);
    }

    // Check if this is a command response
    if (message.contains("commandId"))
    {
        QString commandId =
            message.value("commandId").toString();
        bool success =
            message.value("success").toBool(false);

        // Emit signal for command result
        emit commandResultReceived(commandId, success,
                                   message);

        // Check for errors
        if (!success && message.contains("error"))
        {
            QString errorMsg =
                message.value("error").toString();
            emit errorOccurred(errorMsg);
        }
    }
}

/**
 * Normalizes an event name for consistent lookup.
 */
QString SimulationClientBase::normalizeEventName(
    const QString &eventName)
{
    return eventName.trimmed().toLower().remove(' ');
}

/**
 * Registers an event with the event system.
 */
void SimulationClientBase::registerEvent(
    const QString &eventName, const QJsonObject &eventData)
{
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_eventMutex);
    m_receivedEvents[eventName] = eventData;
    m_eventCondition.wakeAll();

    qDebug() << "Registered event:" << eventName;
}

/**
 * Clears all registered events.
 */
void SimulationClientBase::clearEvents()
{
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_eventMutex);
    m_receivedEvents.clear();
}

/**
 * Handles incoming messages from RabbitMQ.
 */
void SimulationClientBase::handleMessage(
    const QJsonObject &message)
{
    // Print the received message to inspect its content
    qDebug() << "Received message:"
             << QJsonDocument(message).toJson(
                    QJsonDocument::Compact);

    processMessage(message);
}

/**
 * Load RabbitMQ configuration from config file and keychain.
 */
void SimulationClientBase::loadRabbitMQConfig()
{
    QString configPath =
        Utils::findConfigFilePath("rabbitmq.xml");

    QFile file(configPath);
    if (!file.exists())
    {
        qDebug() << "RabbitMQ config file not found at"
                 << configPath << "- using defaults";
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open RabbitMQ config file:"
                   << configPath;
        return;
    }

    QDomDocument doc;
    auto result = doc.setContent(&file);
    file.close();
    if (!result)
    {
        qWarning() << "Failed to parse RabbitMQ config XML:"
                   << result.errorMessage << "at line"
                   << result.errorLine;
        return;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "rabbitmq")
    {
        qWarning()
            << "Invalid RabbitMQ config: root element is not "
               "'rabbitmq'";
        return;
    }

    // Read configuration values
    QDomElement hostElem = root.firstChildElement("host");
    if (!hostElem.isNull())
    {
        m_host = hostElem.text();
    }

    QDomElement portElem = root.firstChildElement("port");
    if (!portElem.isNull())
    {
        bool ok;
        int  port = portElem.text().toInt(&ok);
        if (ok && port > 0 && port <= 65535)
        {
            m_port = port;
        }
    }

    QDomElement usernameElem =
        root.firstChildElement("username");
    if (!usernameElem.isNull())
    {
        m_username = usernameElem.text();
    }

    qDebug() << "Loaded RabbitMQ config: host=" << m_host
             << "port=" << m_port
             << "username=" << m_username;

#ifdef HAVE_QTKEYCHAIN
    // Load password from OS keychain
    QKeychain::ReadPasswordJob job("CargoNetSim");
    job.setAutoDelete(false);
    job.setKey("rabbitmq-password");

    QEventLoop loop;
    QObject::connect(
        &job, &QKeychain::ReadPasswordJob::finished, &loop,
        &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() == QKeychain::NoError)
    {
        m_password = job.textData();
        qDebug() << "Loaded RabbitMQ password from keychain";
    }
    else
    {
        qDebug() << "Could not load password from keychain:"
                 << job.errorString()
                 << "- trying XML fallback";
#endif
        // Fallback: load password from XML file
        QDomElement passwordElem =
            root.firstChildElement("password");
        if (!passwordElem.isNull())
        {
            m_password = passwordElem.text();
            qDebug() << "Loaded RabbitMQ password from config "
                        "file";
        }
        else
        {
            qDebug() << "No password in config - using default";
        }
#ifdef HAVE_QTKEYCHAIN
    }
#endif
}

} // namespace Backend
} // namespace CargoNetSim
