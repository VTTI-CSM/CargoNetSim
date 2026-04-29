#pragma once

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Commons/ClientType.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/LoggerInterface.h"
#include "Backend/Commons/ThreadSafetyUtils.h"
#include <QEventLoop>
#include <QFuture>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPromise>
#include <QQueue>
#include <QReadWriteLock>
#include <QStringList>
#include <QTimer>
#include <QWaitCondition>
#include <atomic>
#include <limits>

// Forward declaration
namespace CargoNetSim
{
namespace Backend
{
class CargoNetSimController;
class SimulationTime;
class TerminalSimulationClient;

} // namespace Backend
} // namespace CargoNetSim

namespace CargoNetSim
{
namespace Backend
{

/**
 * @brief Base class for simulation clients
 *
 * Provides a unified interface for sending commands and
 * processing responses. Ensures commands are processed one
 * at a time with proper waiting for server responses.
 */
class SimulationClientBase : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     * @param host RabbitMQ host
     * @param port RabbitMQ port
     * @param exchange RabbitMQ exchange
     * @param commandQueue Command queue name
     * @param responseQueue Response queue name
     * @param sendingRoutingKey Routing key for sent
     * messages
     * @param receivingRoutingKeys Routing keys for received
     * messages
     * @param clientType Type of client
     */
    explicit SimulationClientBase(
        QObject       *parent = nullptr,
        const QString &host = "localhost", int port = 5672,
        const QString &exchange     = "CargoNetSim.Exchange",
        const QString &commandQueue = "command_queue",
        const QString &responseQueue     = "response_queue",
        const QString &sendingRoutingKey = "default_key",
        const QStringList &receivingRoutingKeys =
            QStringList{"default_key"},
        ClientType clientType = ClientType::TerminalClient);

    /**
     * @brief Destructor
     */
    virtual ~SimulationClientBase();

    /**
     * @brief Initializes the client object in its target
     * thread context.
     *
     * This function is responsible for performing
     * initialization tasks that require the client object
     * to be in its designated thread, such as setting up
     * signal-slot connections or initializing
     * thread-specific resources. It should be called once,
     * after the object has been moved to its target thread
     * and the thread has started.
     *
     * The function performs the following tasks:
     * - Initializes internal state variables that depend on
     * the thread context.
     * - Sets up connections to external resources, such as
     * message handlers or network clients.
     * - Configures any necessary thread-safe callbacks or
     * event handlers.
     *
     * @param simulationTime Pointer to the simulation time
     * object. This object is used to synchronize the
     * simulation time across different clients.
     *
     * @param logger Optional logger interface for logging
     * messages (default is nullptr). If provided, it will
     * be used for logging during initialization. If not
     * provided, no logging will occur. This allows for
     * flexible logging options depending on the client's
     * needs.
     * @note This function is virtual and can be overridden
     * by derived classes to add subclass-specific
     * initialization logic. When overriding, it is
     * recommended to call the base class implementation
     * first.
     *
     * @warning This function should only be called once,
     * after the object has been moved to its target thread
     * and the thread has started. Calling it multiple times
     * or from the wrong thread may result in undefined
     * behavior.
     *
     * @throws std::runtime_error If initialization fails
     * due to resource unavailability or configuration
     * errors.
     *
     * @see SimulationClientBase::SimulationClientBase
     * @see QThread::started
     */
    virtual void initializeClient(
        SimulationTime           *simulationTime,
        TerminalSimulationClient *terminalClient = nullptr,
        LoggerInterface          *logger         = nullptr);

    /**
     * @brief Override the execution time used by live runtime callbacks
     *
     * Thread-safe. Intended for the new executor-owned live execution loop.
     * When set, derived clients should prefer this value over the legacy
     * shared SimulationTime object for event timestamps and terminal handoff
     * timestamps.
     */
    void setExecutionTimeOverride(double currentTimeSeconds);

    /**
     * @brief Clear the live execution time override
     *
     * Thread-safe. After clearing, derived clients fall back to the shared
     * SimulationTime object when present.
     */
    void clearExecutionTimeOverride();

    /**
     * @brief Sets the controller reference
     * @param controller Pointer to controller
     */
    virtual void
    setController(CargoNetSimController *controller);

    /**
     * @brief Checks if client is connected to server
     * @return True if connected
     */
    bool isConnected() const;

    /**
     * @brief Connects to the simulation server
     * @return True if connection was successful
     */
    bool connectToServer();

    /**
     * @brief Disconnects from the simulation server
     */
    void disconnectFromServer();

    /**
     * @brief Get client type enumeration
     * @return The client type
     */
    ClientType getClientType() const;

    /**
     * @brief Get client type as string
     * @return String representation of client type
     */
    QString getClientTypeString() const;

    /**
     * @brief Get the RabbitMQ handler
     * @return Pointer to the RabbitMQ handler
     */
    RabbitMQHandler *getRabbitMQHandler() const
    {
        return m_rabbitMQHandler;
    }

    /**
     * @brief Probe whether the simulator can accept and answer commands
     *
     * This is an application-level readiness probe, distinct from the
     * lower-level RabbitMQ connection and queue-consumer diagnostics.
     * Derived classes may override it when the simulator exposes a
     * different lightweight probe command than the default
     * `checkConnection`.
     *
     * @param timeoutMs Maximum time to wait for the probe response
     * @return True when the simulator answered the probe successfully
     */
    virtual bool probeCommandAvailability(int timeoutMs = 500);

signals:
    /**
     * @brief Emitted when an event is received
     * @param event Event name (normalized)
     * @param data Event data as JSON
     */
    void eventReceived(const QString     &event,
                       const QJsonObject &data);

    /**
     * @brief Emitted when a command is sent
     * @param commandId Unique ID of the command
     * @param command Command name
     */
    void commandSent(const QString &commandId,
                     const QString &command);

    /**
     * @brief Emitted when a command result is received
     * @param commandId Unique ID of the command
     * @param success Whether the command was successful
     * @param result Result data
     */
    void commandResultReceived(const QString     &commandId,
                               bool               success,
                               const QJsonObject &result);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Error message
     */
    void errorOccurred(const QString &errorMessage);

    /**
     * @brief Emitted when connection status changes
     * @param connected True if connected, false otherwise
     */
    void connectionStatusChanged(bool connected);

protected:
    /**
     * @brief Resolve the current execution time for runtime callbacks
     *
     * Resolution order:
     * 1. executor-owned override when present
     * 2. shared SimulationTime when present
     * 3. `-1.0` when no time source is available
     */
    double currentExecutionTime() const;

    /**
     * @brief Send a command and wait for a specific
     * response event
     *
     * This method sends a command and blocks until the
     * expected response event is received or a timeout
     * occurs.
     *
     * @param command Command name
     * @param params Command parameters (optional)
     * @param expectedEvents Events to wait for (at least
     * one needed)
     * @param timeoutMs Timeout in milliseconds (use -1 for
     * no timeout)
     * @param routingKey Custom routing key (optional)
     * @return True if command was processed successfully
     */
    virtual bool sendCommandAndWait(
        const QString &command, const QJsonObject &params,
        const QStringList &expectedEvents,
        int                timeoutMs  = 7200000, // 2 hour
        const QString     &routingKey = QString());

    /**
     * @brief Send a command without waiting for response
     * @param command Command name
     * @param params Command parameters (optional)
     * @param routingKey Custom routing key (optional)
     * @param sendAsText Whether to send the command as text
     * or json (optional). If this is set to true, no params
     * will be considered.
     * @return True if command was sent successfully
     */
    virtual bool
    sendCommand(const QString     &command,
                const QJsonObject &params = QJsonObject(),
                const QString     &routingKey = QString(),
                bool               sendAsText = false);

    /**
     * @brief Creates a command object with parameters
     *
     * Override this in derived classes to add
     * client-specific fields to command objects.
     *
     * @param command Command name
     * @param params Command parameters
     * @return Command object as JSON
     */
    virtual QJsonObject createCommandObject(
        const QString     &command,
        const QJsonObject &params = QJsonObject());

    /**
     * @brief Wait for a specific event to occur
     *
     * This method blocks until one of the expected events
     * is received or a timeout occurs.
     *
     * @param expectedEvents List of events to wait for
     * @param timeoutMs Timeout in milliseconds (-1 for
     * infinite)
     * @return True if an event was received before timeout
     */
    virtual bool
    waitForEvent(const QStringList &expectedEvents,
                 int                timeoutMs = -1);

    /**
     * @brief Check if a specific event has been received
     * @param eventName Name of the event to check
     * @return True if the event has been received
     */
    bool hasReceivedEvent(const QString &eventName) const;

    /**
     * @brief Get data for a received event
     * @param eventName Name of the event
     * @return Event data as JSON object
     */
    QJsonObject
    getEventData(const QString &eventName) const;

    /**
     * @brief Process message from server
     *
     * This method determines if a message is a command
     * response or an event and routes it accordingly.
     * Override in derived classes to add custom processing.
     *
     * @param message Message JSON object
     */
    virtual void processMessage(const QJsonObject &message);

    /**
     * @brief Hook for subclasses to populate typed caches
     *        from an incoming event.
     *
     * Invoked by `processMessage` on the I/O thread *before*
     * the event is registered in `m_receivedEvents` and
     * before any `eventReceived` / `commandResultReceived`
     * signal is emitted. Subclasses override this to mutate
     * their per-event caches so that threads blocked in
     * `sendCommandAndWait` / `waitForEvent` observe the
     * populated state the moment they wake.
     *
     * Default implementation is a no-op.
     *
     * @param normalizedEvent  Event name after normalization
     * @param message          Full message JSON object
     */
    virtual void
    onEventReceived(const QString     &normalizedEvent,
                    const QJsonObject &message);

    /**
     * @brief Normalize an event name
     * @param eventName Event name to normalize
     * @return Normalized event name
     */
    static QString
    normalizeEventName(const QString &eventName);

    /**
     * @brief Register a received event
     * @param eventName Name of the event (normalized)
     * @param eventData Event data
     */
    void registerEvent(const QString     &eventName,
                       const QJsonObject &eventData);

    /**
     * @brief Clear all registered events
     */
    void clearEvents();

    /**
     * @brief Execute a function while ensuring serialized
     * command execution
     *
     * This method ensures that only one command is
     * processed at a time. All command execution should go
     * through this method.
     *
     * @param func Function to execute
     * @return Result of the function
     */
    template <typename Func>
    auto executeSerializedCommand(Func func)
        -> decltype(func())
    {
        // Check if m_rabbitMQHandler exists and is
        // connected
        if (m_rabbitMQHandler == nullptr || !isConnected())
        {
            qCWarning(lcClient)
                << "Cannot execute command: RabbitMQ "
                   "handler"
                   " not initialized or not connected";
            throw std::runtime_error("Client not ready for "
                                     "command execution");
        }

        CargoNetSim::Backend::Commons::ScopedWriteLock
            locker(m_commandSerializationMutex);
        return func();
    }

    // RabbitMQ handler
    RabbitMQHandler *m_rabbitMQHandler = nullptr;

    // Client type identification
    ClientType m_clientType;

    // Event registry for synchronization
    QMap<QString, QJsonObject> m_receivedEvents;
    mutable QReadWriteLock     m_eventMutex;
    QWaitCondition             m_eventCondition;

    // Connection parameters
    QString     m_host;
    int         m_port;
    QString     m_username = "guest";
    QString     m_password = "guest";
    QString     m_exchange;
    QString     m_commandQueue;
    QString     m_responseQueue;
    QString     m_sendingRoutingKey;
    QStringList m_receivingRoutingKeys;

    // Logging interface
    LoggerInterface *m_logger = nullptr;

    // SimulationTime interface
    SimulationTime *m_simulationTime = nullptr;

    // Terminal client reference
    TerminalSimulationClient *m_terminalClient = nullptr;

    // Controller
    CargoNetSimController *m_controller = nullptr;

    // Command timeout constant
    static const int COMMAND_TIMEOUT_MS =
        1800000; // 30 minutes

private slots:
    /**
     * @brief Handle messages from RabbitMQ
     * @param message Message JSON object
     */
    void handleMessage(const QJsonObject &message);

private:
    /**
     * @brief Load RabbitMQ configuration from config file
     *
     * Loads host, port, username, exchange from XML config.
     * Password is loaded from OS keychain.
     */
    void loadRabbitMQConfig();

    // Command serialization
    QReadWriteLock m_commandSerializationMutex;

    // Currently processing flag for preventing concurrent
    // operations
    std::atomic<bool> m_processingCommand;

    // Live executor time override. NaN means "no override".
    std::atomic<double> m_executionTimeOverrideSeconds{
        std::numeric_limits<double>::quiet_NaN()};
};

} // namespace Backend
} // namespace CargoNetSim

Q_DECLARE_METATYPE(
    CargoNetSim::Backend::SimulationClientBase)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::SimulationClientBase *)
