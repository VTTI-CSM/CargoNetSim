#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <atomic>
#include <rabbitmq-c/amqp.h>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @brief Handles RabbitMQ communication for the simulation
 *
 * Manages connections to RabbitMQ, message sending and
 * receiving, and connection maintenance through heartbeats.
 */
class RabbitMQHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     * @param host RabbitMQ host
     * @param port RabbitMQ port
     * @param username RabbitMQ username
     * @param password RabbitMQ password
     * @param exchange RabbitMQ exchange name
     * @param commandQueue Command queue name
     * @param responseQueue Response queue name
     * @param sendingRoutingKey Default sending routing key
     * @param receivingRoutingKeys List of routing keys to
     *                             receive on
     */
    explicit RabbitMQHandler(
        QObject       *parent = nullptr,
        const QString &host = "localhost", int port = 5672,
        const QString &username = "guest",
        const QString &password = "guest",
        const QString &exchange     = "CargoNetSim.Exchange",
        const QString &commandQueue = "command_queue",
        const QString &responseQueue     = "response_queue",
        const QString &sendingRoutingKey = "default_key",
        const QStringList &receivingRoutingKeys =
            QStringList{"default_key"});

    /**
     * @brief Destructor
     */
    virtual ~RabbitMQHandler();

    /**
     * @brief Establishes connection to RabbitMQ
     * @return True if connection successful
     */
    bool establishConnection();

    /**
     * @brief Disconnects from RabbitMQ
     */
    void disconnect();

    /**
     * @brief Checks if connected to RabbitMQ
     * @return True if connected
     */
    bool isConnected() const;

    /**
     * @brief Sends a command message to RabbitMQ
     * @param message JSON message to send
     * @param routingKey Routing key to use (optional)
     * @return True if message sent successfully
     */
    bool sendCommand(const QJsonObject &message,
                     const QString &routingKey = QString());

    /**
     * @brief Sends a command message to RabbitMQ as a plain
     * string
     * @param messageStr String message to send (any format)
     * @param routingKey Routing key to use (optional)
     * @return True if message sent successfully
     */
    bool sendCommand(const QString &messageStr,
                     const QString &routingKey = QString());

    /**
     * @brief Starts consuming messages from response queue
     */
    void startConsuming();

    /**
     * @brief Stops consuming messages from response queue
     */
    void stopConsuming();

    /**
     * @brief Sets up heartbeat mechanism
     * @param heartbeatInterval Interval in seconds between
     *                          heartbeats
     */
    void setupHeartbeat(int heartbeatInterval = 5);

    /**
     * @brief Stops the heartbeat mechanism
     */
    void stopHeartbeat();

    /**
     * @brief Checks if there are any consumers for a
     * specific queue
     * @param queueName The name of the queue to check
     * @return True if there are active consumers, false
     * otherwise
     */
    bool hasConsumers(const QString &queueName);

    /**
     * @brief Checks if there are any consumers for the
     * command queue
     * @return True if there are active consumers, false
     * otherwise
     */
    bool hasCommandQueueConsumers();

    /**
     * @brief Checks if there are any consumers for the
     * response queue
     * @return True if there are active consumers, false
     * otherwise
     */
    bool hasResponseQueueConsumers();

    /**
     * @brief Gets the command queue name
     * @return The name of the command queue
     */
    QString getCommandQueueName() const
    {
        return m_commandQueue;
    }

    QString getResponseQueueName() const
    {
        return m_responseQueue;
    }

    QString getPrimaryReceivingRoutingKey() const
    {
        return m_receivingRoutingKeys.isEmpty()
                   ? QString()
                   : m_receivingRoutingKeys.constFirst();
    }

signals:
    /**
     * @brief Emitted when a message is received
     * @param message JSON message received
     */
    void messageReceived(const QJsonObject &message);

    /**
     * @brief Emitted when connection status changes
     * @param connected True if connected, false otherwise
     */
    void connectionChanged(bool connected);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Error message
     */
    void errorOccurred(const QString &errorMessage);

private slots:
    /**
     * @brief Consumer thread function
     */
    void workerThreadFunction();

    /**
     * @brief Heartbeat thread function
     * @param interval Heartbeat interval in seconds
     */
    void heartbeatThreadFunction(int interval);

private:
    /**
     * @brief Sets up the RabbitMQ exchange
     * @return True if setup successful
     */
    bool setupExchange();

    /**
     * @brief Sets up the command and response queues
     * @return True if setup successful
     */
    bool setupQueues();

    /**
     * @brief Binds queues to exchange with routing keys
     * @return True if binding successful
     */
    bool bindQueues();

    /**
     * @brief Processes incoming messages
     */
    void processMessages();

    /**
     * @brief Internal helper method to send messages to
     * RabbitMQ
     * @param data The raw message data to send
     * @param contentType The MIME content type of the
     * message
     * @param messageId The message ID to use (or empty to
     * generate one)
     * @param routingKey The routing key to use (optional)
     * @return True if message sent successfully
     */
    bool sendMessage(const QByteArray &data,
                     const QString    &contentType,
                     const QString    &messageId,
                     const QString    &routingKey);

    /**
     * @brief Reconnects the sending connection
     */
    void reconnectSending();

    /**
     * @brief Reconnects the receiving connection
     */
    void reconnectReceiving();

    // RabbitMQ connection state
    amqp_connection_state_t m_sendConnection;
    amqp_connection_state_t m_receiveConnection;
    bool                    m_connected;

    // Connection parameters
    QString     m_host;
    int         m_port;
    QString     m_username;
    QString     m_password;
    QString     m_exchange;
    QString     m_commandQueue;
    QString     m_responseQueue;
    QString     m_sendingRoutingKey;
    QStringList m_receivingRoutingKeys;

    // Threads
    QThread          *m_consumerThread;
    QThread          *m_heartbeatThread;
    std::atomic<bool> m_threadRunning;
    std::atomic<bool> m_heartbeatActive;
    qint64            m_lastHeartbeatSent;

    // Thread safety
    mutable QMutex m_mutex;

    // Constants
    static const int MAX_RETRIES = 5;
};

} // namespace Backend
} // namespace CargoNetSim

Q_DECLARE_METATYPE(CargoNetSim::Backend::RabbitMQHandler)
Q_DECLARE_METATYPE(CargoNetSim::Backend::RabbitMQHandler *)
