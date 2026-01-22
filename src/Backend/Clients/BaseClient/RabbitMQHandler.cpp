#include "RabbitMQHandler.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <chrono>
#include <rabbitmq-c/tcp_socket.h>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

namespace CargoNetSim
{
namespace Backend
{

/**
 * Constructor initializes the RabbitMQ handler with
 * connection parameters and sets up the initial state.
 */
RabbitMQHandler::RabbitMQHandler(
    QObject *parent, const QString &host, int port,
    const QString &username, const QString &password,
    const QString &exchange, const QString &commandQueue,
    const QString     &responseQueue,
    const QString     &sendingRoutingKey,
    const QStringList &receivingRoutingKeys)
    : QObject(parent)
    , m_sendConnection(nullptr)
    , m_receiveConnection(nullptr)
    , m_connected(false)
    , m_host(host)
    , m_port(port)
    , m_username(username)
    , m_password(password)
    , m_exchange(exchange)
    , m_commandQueue(commandQueue)
    , m_responseQueue(responseQueue)
    , m_sendingRoutingKey(sendingRoutingKey)
    , m_receivingRoutingKeys(receivingRoutingKeys)
    , m_consumerThread(nullptr)
    , m_heartbeatThread(nullptr)
    , m_threadRunning(false)
    , m_heartbeatActive(false)
    , m_lastHeartbeatSent(0)
{
    qDebug() << "RabbitMQ handler initialized with:"
             << "exchange:" << m_exchange
             << "command queue:" << m_commandQueue
             << "response queue:" << m_responseQueue;
}

/**
 * Destructor ensures proper cleanup of resources.
 */
RabbitMQHandler::~RabbitMQHandler()
{
    stopHeartbeat();
    disconnect();
    qDebug() << "RabbitMQ handler destroyed";
}

/**
 * Establishes connections to RabbitMQ for both sending
 * and receiving messages.
 *
 * @return True if connection was successful
 */
bool RabbitMQHandler::establishConnection()
{
    QMutexLocker locker(&m_mutex);

    if (m_connected)
    {
        qDebug() << "Already connected to RabbitMQ";
        return true;
    }

    qDebug() << "Connecting to RabbitMQ at" << m_host << ":"
             << m_port;

    // Try multiple connect attempts with retry logic
    int retryCount = 0;
    while (retryCount < MAX_RETRIES)
    {
        try
        {
            // Setup sending connection
            m_sendConnection = amqp_new_connection();
            if (!m_sendConnection)
            {
                qWarning()
                    << "Failed to create send connection";
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Create and open socket for sending
            amqp_socket_t *sendSocket =
                amqp_tcp_socket_new(m_sendConnection);
            if (!sendSocket)
            {
                qWarning()
                    << "Failed to create send socket";
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Open send socket
            int status = amqp_socket_open(
                sendSocket, m_host.toUtf8().constData(),
                m_port);
            if (status != AMQP_STATUS_OK)
            {
                qWarning() << "Failed to open send socket: "
                           << status;
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Login to send connection
            amqp_rpc_reply_t sendLoginReply =
                amqp_login(m_sendConnection,
                           "/",    // vhost
                           0,      // channel max
                           131072, // frame max
                           0,      // heartbeat
                           AMQP_SASL_METHOD_PLAIN,
                           m_username.toUtf8().constData(),
                           m_password.toUtf8().constData());

            if (sendLoginReply.reply_type
                != AMQP_RESPONSE_NORMAL)
            {
                qWarning()
                    << "Failed to login to send connection";
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Open send channel
            amqp_channel_open(m_sendConnection, 1);
            amqp_rpc_reply_t sendChannelReply =
                amqp_get_rpc_reply(m_sendConnection);
            if (sendChannelReply.reply_type
                != AMQP_RESPONSE_NORMAL)
            {
                qWarning() << "Failed to open send channel";
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Now setup receiving connection
            m_receiveConnection = amqp_new_connection();
            if (!m_receiveConnection)
            {
                qWarning() << "Failed to create receive "
                              "connection";
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Create and open socket for receiving
            amqp_socket_t *receiveSocket =
                amqp_tcp_socket_new(m_receiveConnection);
            if (!receiveSocket)
            {
                qWarning()
                    << "Failed to create receive socket";
                amqp_destroy_connection(m_sendConnection);
                amqp_destroy_connection(
                    m_receiveConnection);
                m_sendConnection    = nullptr;
                m_receiveConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Open receive socket
            status = amqp_socket_open(
                receiveSocket, m_host.toUtf8().constData(),
                m_port);
            if (status != AMQP_STATUS_OK)
            {
                qWarning()
                    << "Failed to open receive socket: "
                    << status;
                amqp_destroy_connection(m_sendConnection);
                amqp_destroy_connection(
                    m_receiveConnection);
                m_sendConnection    = nullptr;
                m_receiveConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Login to receive connection
            amqp_rpc_reply_t receiveLoginReply =
                amqp_login(m_receiveConnection,
                           "/",    // vhost
                           0,      // channel max
                           131072, // frame max
                           0,      // heartbeat
                           AMQP_SASL_METHOD_PLAIN,
                           m_username.toUtf8().constData(),
                           m_password.toUtf8().constData());

            if (receiveLoginReply.reply_type
                != AMQP_RESPONSE_NORMAL)
            {
                qWarning() << "Failed to login to receive "
                              "connection";
                amqp_destroy_connection(m_sendConnection);
                amqp_destroy_connection(
                    m_receiveConnection);
                m_sendConnection    = nullptr;
                m_receiveConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Open receive channel
            amqp_channel_open(m_receiveConnection, 1);
            amqp_rpc_reply_t receiveChannelReply =
                amqp_get_rpc_reply(m_receiveConnection);
            if (receiveChannelReply.reply_type
                != AMQP_RESPONSE_NORMAL)
            {
                qWarning()
                    << "Failed to open receive channel";
                amqp_destroy_connection(m_sendConnection);
                amqp_destroy_connection(
                    m_receiveConnection);
                m_sendConnection    = nullptr;
                m_receiveConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // Setup exchange and queues
            if (!setupExchange() || !setupQueues()
                || !bindQueues())
            {
                qWarning()
                    << "Failed to setup exchange or queues";
                amqp_destroy_connection(m_sendConnection);
                amqp_destroy_connection(
                    m_receiveConnection);
                m_sendConnection    = nullptr;
                m_receiveConnection = nullptr;
                retryCount++;
                std::this_thread::sleep_for(
                    std::chrono::seconds(2 * retryCount));
                continue;
            }

            // If we got here, connection was successful
            m_connected = true;
            emit connectionChanged(true);

            // Start consumer thread for receiving messages
            m_threadRunning  = true;
            m_consumerThread = new QThread();
            this->moveToThread(m_consumerThread);

            connect(m_consumerThread, &QThread::started,
                    this,
                    &RabbitMQHandler::workerThreadFunction);
            connect(m_consumerThread, &QThread::finished,
                    this,
                    [this]() { m_threadRunning = false; });

            m_consumerThread->start();

            qDebug()
                << "Successfully connected to RabbitMQ at"
                << m_host << ":" << m_port;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning()
                << "Exception during RabbitMQ connection:"
                << e.what();

            if (m_sendConnection)
            {
                amqp_destroy_connection(m_sendConnection);
                m_sendConnection = nullptr;
            }

            if (m_receiveConnection)
            {
                amqp_destroy_connection(
                    m_receiveConnection);
                m_receiveConnection = nullptr;
            }

            m_connected = false;
            retryCount++;
            std::this_thread::sleep_for(
                std::chrono::seconds(2 * retryCount));
        }
    }

    qWarning() << "Failed to connect to RabbitMQ after"
               << MAX_RETRIES << "attempts";
    return false;
}

/**
 * Disconnects from RabbitMQ and cleans up resources.
 */
void RabbitMQHandler::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected)
    {
        return;
    }

    qDebug() << "Disconnecting from RabbitMQ";

    // Stop worker thread
    m_threadRunning = false;

    if (m_consumerThread)
    {
        if (m_consumerThread->isRunning())
        {
            m_consumerThread->quit();
            m_consumerThread->wait(
                2000); // Wait up to 2 seconds

            if (m_consumerThread->isRunning())
            {
                qWarning()
                    << "Consumer thread didn't exit, "
                    << "terminating";
                m_consumerThread->terminate();
            }
        }

        delete m_consumerThread;
        m_consumerThread = nullptr;
    }

    // Close send connection
    if (m_sendConnection)
    {
        try
        {
            amqp_channel_close(m_sendConnection, 1,
                               AMQP_REPLY_SUCCESS);
            amqp_connection_close(m_sendConnection,
                                  AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(m_sendConnection);
        }
        catch (const std::exception &e)
        {
            qWarning() << "Error closing send connection:"
                       << e.what();
        }
        m_sendConnection = nullptr;
    }

    // Close receive connection
    if (m_receiveConnection)
    {
        try
        {
            amqp_channel_close(m_receiveConnection, 1,
                               AMQP_REPLY_SUCCESS);
            amqp_connection_close(m_receiveConnection,
                                  AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(m_receiveConnection);
        }
        catch (const std::exception &e)
        {
            qWarning()
                << "Error closing receive connection:"
                << e.what();
        }
        m_receiveConnection = nullptr;
    }

    m_connected = false;
    emit connectionChanged(false);

    qDebug() << "Disconnected from RabbitMQ";
}

/**
 * Checks if the handler is currently connected to RabbitMQ.
 *
 * @return True if connected
 */
bool RabbitMQHandler::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_connected && m_sendConnection != nullptr
           && m_receiveConnection != nullptr;
}

/**
 * @brief Sends a command message to RabbitMQ
 * @param message JSON message to send
 * @param routingKey Routing key to use (optional)
 * @return True if message sent successfully
 */
bool RabbitMQHandler::sendCommand(
    const QJsonObject &message, const QString &routingKey)
{
    // Convert message to JSON string
    QJsonDocument doc(message);
    QByteArray    data = doc.toJson(QJsonDocument::Compact);

    // Extract message ID if it exists
    QString messageId =
        message.contains("messageId")
            ? message["messageId"].toString()
            : QString();

    return sendMessage(data, "application/json", messageId,
                       routingKey);
}

/**
 * @brief Sends a command message to RabbitMQ as a plain
 * string
 * @param messageStr String message to send (any format)
 * @param routingKey Routing key to use (optional)
 * @return True if message sent successfully
 */
bool RabbitMQHandler::sendCommand(const QString &messageStr,
                                  const QString &routingKey)
{
    // Convert message to bytes
    QByteArray data = messageStr.toUtf8();

    return sendMessage(data, "text/plain", QString(),
                       routingKey);
}

/**
 * Sets up the exchange for RabbitMQ communication.
 *
 * @return True if the exchange was set up successfully
 */
bool RabbitMQHandler::setupExchange()
{
    try
    {
        // Declare exchange for sending
        amqp_exchange_declare(
            m_sendConnection,
            1, // channel
            amqp_cstring_bytes(
                m_exchange.toUtf8().constData()),
            amqp_cstring_bytes("topic"),
            0, // passive
            1, // durable
            0, // auto delete
            0, // internal
            amqp_empty_table);

        amqp_rpc_reply_t reply =
            amqp_get_rpc_reply(m_sendConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning()
                << "Failed to declare exchange for sending";
            return false;
        }

        // Declare exchange for receiving
        amqp_exchange_declare(
            m_receiveConnection,
            1, // channel
            amqp_cstring_bytes(
                m_exchange.toUtf8().constData()),
            amqp_cstring_bytes("topic"),
            0, // passive
            1, // durable
            0, // auto delete
            0, // internal
            amqp_empty_table);

        reply = amqp_get_rpc_reply(m_receiveConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to declare exchange for "
                          "receiving";
            return false;
        }

        qDebug() << "Exchange declared:" << m_exchange;
        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception during exchange setup:"
                   << e.what();
        return false;
    }
}

/**
 * Sets up the command and response queues for RabbitMQ
 * communication.
 *
 * @return True if the queues were set up successfully
 */
bool RabbitMQHandler::setupQueues()
{
    try
    {
        // Declare command queue
        amqp_queue_declare(
            m_sendConnection,
            1, // channel
            amqp_cstring_bytes(
                m_commandQueue.toUtf8().constData()),
            0, // passive
            1, // durable
            0, // exclusive
            0, // auto delete
            amqp_empty_table);

        amqp_rpc_reply_t reply =
            amqp_get_rpc_reply(m_sendConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to declare command queue";
            return false;
        }

        // Declare response queue
        amqp_queue_declare(
            m_receiveConnection,
            1, // channel
            amqp_cstring_bytes(
                m_responseQueue.toUtf8().constData()),
            0, // passive
            1, // durable
            0, // exclusive
            0, // auto delete
            amqp_empty_table);

        reply = amqp_get_rpc_reply(m_receiveConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning()
                << "Failed to declare response queue";
            return false;
        }

        qDebug() << "Queues declared:" << m_commandQueue
                 << "and" << m_responseQueue;
        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception during queue setup:"
                   << e.what();
        return false;
    }
}

/**
 * Binds the queues to the exchange with appropriate
 * routing keys.
 *
 * @return True if the bindings were successful
 */
bool RabbitMQHandler::bindQueues()
{
    try
    {
        // Bind command queue with sending routing key
        amqp_queue_bind(
            m_sendConnection,
            1, // channel
            amqp_cstring_bytes(
                m_commandQueue.toUtf8().constData()),
            amqp_cstring_bytes(
                m_exchange.toUtf8().constData()),
            amqp_cstring_bytes(
                m_sendingRoutingKey.toUtf8().constData()),
            amqp_empty_table);

        amqp_rpc_reply_t reply =
            amqp_get_rpc_reply(m_sendConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to bind command queue";
            return false;
        }

        // Bind response queue with each receiving routing
        // key
        for (const QString &key : m_receivingRoutingKeys)
        {
            amqp_queue_bind(
                m_receiveConnection,
                1, // channel
                amqp_cstring_bytes(
                    m_responseQueue.toUtf8().constData()),
                amqp_cstring_bytes(
                    m_exchange.toUtf8().constData()),
                amqp_cstring_bytes(
                    key.toUtf8().constData()),
                amqp_empty_table);

            reply = amqp_get_rpc_reply(m_receiveConnection);
            if (reply.reply_type != AMQP_RESPONSE_NORMAL)
            {
                qWarning() << "Failed to bind response "
                              "queue with key:"
                           << key;
                return false;
            }
        }

        qDebug() << "Queues bound: command queue to"
                 << m_sendingRoutingKey
                 << "and response queue to"
                 << m_receivingRoutingKeys.join(", ");
        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception during queue binding:"
                   << e.what();
        return false;
    }
}

/**
 * Starts consuming messages from the response queue.
 */
void RabbitMQHandler::startConsuming()
{
    try
    {
        // Start consuming from response queue
        amqp_basic_consume(
            m_receiveConnection,
            1, // channel
            amqp_cstring_bytes(
                m_responseQueue.toUtf8().constData()),
            amqp_empty_bytes, // consumer tag
                              // (server-generated)
            0,                // no local
            1,                // no ack - auto acknowledge
            0,                // exclusive
            amqp_empty_table);

        amqp_rpc_reply_t reply =
            amqp_get_rpc_reply(m_receiveConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to start consuming";
            return;
        }

        qDebug() << "Started consuming from response queue:"
                 << m_responseQueue;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception during start consuming:"
                   << e.what();
    }
}

/**
 * Stops consuming messages from the response queue.
 */
void RabbitMQHandler::stopConsuming()
{
    if (m_receiveConnection && m_connected)
    {
        try
        {
            amqp_channel_close(m_receiveConnection, 1,
                               AMQP_REPLY_SUCCESS);
            qDebug() << "Stopped consuming messages";
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception during stop consuming:"
                       << e.what();
        }
    }
}

/**
 * Processes incoming messages from the response queue.
 */
void RabbitMQHandler::processMessages()
{
    try
    {
        // Set timeout for receiving messages
        struct timeval timeout;
        timeout.tv_sec  = 1; // 1 second timeout
        timeout.tv_usec = 0;

        // Receive message with timeout
        amqp_envelope_t  envelope;
        amqp_rpc_reply_t result = amqp_consume_message(
            m_receiveConnection, &envelope, &timeout, 0);

        // Check result
        if (result.reply_type == AMQP_RESPONSE_NORMAL)
        {
            // Process message
            if (envelope.message.body.len > 0)
            {
                // Convert message body to QByteArray
                QByteArray messageData(
                    static_cast<char *>(
                        envelope.message.body.bytes),
                    envelope.message.body.len);

                // Parse JSON
                QJsonParseError error;
                QJsonDocument doc = QJsonDocument::fromJson(
                    messageData, &error);

                if (error.error == QJsonParseError::NoError
                    && doc.isObject())
                {
                    QJsonObject message = doc.object();

                    // Add message ID if available in
                    // properties
                    if (envelope.message.properties._flags
                        & AMQP_BASIC_MESSAGE_ID_FLAG)
                    {
                        QByteArray messageId(
                            static_cast<char *>(
                                envelope.message.properties
                                    .message_id.bytes),
                            envelope.message.properties
                                .message_id.len);
                        message["messageId"] =
                            QString::fromUtf8(messageId);
                    }

                    // Add routing key to message
                    QString routingKey = QString::fromUtf8(
                        static_cast<char *>(
                            envelope.routing_key.bytes),
                        envelope.routing_key.len);
                    message["routingKey"] = routingKey;

                    // Emit signal with message
                    qDebug() << "Received message with "
                                "routing key:"
                             << routingKey
                             << " and command event: "
                             << message["event"].toString();
                    ;
                    emit messageReceived(message);
                }
                else
                {
                    qWarning()
                        << "Error parsing message JSON:"
                        << error.errorString();
                }
            }

            // Release envelope resources
            amqp_destroy_envelope(&envelope);
        }
        else if (result.reply_type
                     == AMQP_RESPONSE_LIBRARY_EXCEPTION
                 && result.library_error
                        == AMQP_STATUS_TIMEOUT)
        {
            // Timeout - normal case, no message available
        }
        else
        {
            // Other error
            qWarning()
                << "Error receiving message, reply type:"
                << result.reply_type;

            // If connection lost, try to reconnect
            if (result.reply_type
                    == AMQP_RESPONSE_LIBRARY_EXCEPTION
                && result.library_error
                       == AMQP_STATUS_CONNECTION_CLOSED)
            {
                qWarning() << "Connection closed, "
                              "attempting to reconnect";
                reconnectReceiving();
            }
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception during message processing:"
                   << e.what();
    }
}

/**
 * @brief Internal helper method to send messages to
 * RabbitMQ
 * @param data The raw message data to send
 * @param contentType The MIME content type of the message
 * @param messageId The message ID to use (or empty to
 * generate one)
 * @param routingKey The routing key to use (optional)
 * @return True if message sent successfully
 */
bool RabbitMQHandler::sendMessage(
    const QByteArray &data, const QString &contentType,
    const QString &messageId, const QString &routingKey)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected || !m_sendConnection)
    {
        qWarning() << "Cannot send message: not connected";
        return false;
    }

    QString useRoutingKey = routingKey.isEmpty()
                                ? m_sendingRoutingKey
                                : routingKey;

    int retryCount = 0;
    while (retryCount < MAX_RETRIES)
    {
        try
        {
            // Create message properties
            amqp_basic_properties_t props;
            props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG
                           | AMQP_BASIC_DELIVERY_MODE_FLAG
                           | AMQP_BASIC_MESSAGE_ID_FLAG;
            props.content_type = amqp_cstring_bytes(
                contentType.toUtf8().constData());
            props.delivery_mode = 2; // persistent

            // Generate or use provided message ID
            QString useMessageId =
                messageId.isEmpty()
                    ? QUuid::createUuid().toString()
                    : messageId;
            QByteArray messageIdBytes =
                useMessageId.toUtf8();
            props.message_id =
                amqp_bytes_malloc_dup(amqp_cstring_bytes(
                    messageIdBytes.constData()));

            // Publish message
            int status = amqp_basic_publish(
                m_sendConnection,
                1, // channel
                amqp_cstring_bytes(
                    m_exchange.toUtf8().constData()),
                amqp_cstring_bytes(
                    useRoutingKey.toUtf8().constData()),
                1, // mandatory
                0, // immediate
                &props,
                amqp_bytes_malloc_dup(
                    amqp_cstring_bytes(data.constData())));

            // Free allocated memory
            amqp_bytes_free(props.message_id);

            if (status != AMQP_STATUS_OK)
            {
                qWarning() << "Failed to publish message: "
                           << status;
                retryCount++;

                if (retryCount < MAX_RETRIES)
                {
                    QThread::msleep(500 * retryCount);
                    reconnectSending();
                    continue;
                }
                else
                {
                    return false;
                }
            }

            qDebug() << "Sent message to" << useRoutingKey
                     << "with size" << data.size()
                     << "bytes";
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning()
                << "Exception during message publish:"
                << e.what();
            retryCount++;

            if (retryCount < MAX_RETRIES)
            {
                QThread::msleep(500 * retryCount);
                reconnectSending();
            }
            else
            {
                return false;
            }
        }
    }

    qWarning() << "Failed to send message after"
               << MAX_RETRIES << "attempts";
    return false;
}

/**
 * Worker thread function for consuming messages.
 */
void RabbitMQHandler::workerThreadFunction()
{
    try
    {
        // Start consuming messages
        startConsuming();

        // Process messages until thread is stopped
        while (m_threadRunning)
        {
            processMessages();
            QThread::msleep(
                10); // Small delay to reduce CPU usage
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception in worker thread:"
                   << e.what();
    }

    qDebug() << "Worker thread terminating";
}

/**
 * Heartbeat thread function for maintaining connection.
 *
 * @param interval Interval in seconds between heartbeats
 */
void RabbitMQHandler::heartbeatThreadFunction(int interval)
{
    try
    {
        qDebug() << "Heartbeat thread started with interval"
                 << interval << "seconds";

        while (m_heartbeatActive)
        {
            // Check if connection is alive
            if (m_connected && m_sendConnection)
            {
                // Create heartbeat message
                QJsonObject heartbeat;
                heartbeat["event"] = "heartbeat";
                heartbeat["timestamp"] =
                    QDateTime::currentDateTime()
                        .toMSecsSinceEpoch();

                QJsonDocument doc(heartbeat);
                QByteArray    data =
                    doc.toJson(QJsonDocument::Compact);

                // Special routing key for heartbeats
                QString heartbeatRoutingKey =
                    m_sendingRoutingKey + ".heartbeat";

                // Try to publish heartbeat
                try
                {
                    // Create message properties
                    amqp_basic_properties_t props;
                    props._flags =
                        AMQP_BASIC_CONTENT_TYPE_FLAG
                        | AMQP_BASIC_EXPIRATION_FLAG;
                    props.content_type = amqp_cstring_bytes(
                        "application/json");
                    props.expiration =
                        amqp_cstring_bytes("10000");

                    // Publish heartbeat
                    int status = amqp_basic_publish(
                        m_sendConnection,
                        1, // channel
                        amqp_cstring_bytes(
                            m_exchange.toUtf8()
                                .constData()),
                        amqp_cstring_bytes(
                            heartbeatRoutingKey.toUtf8()
                                .constData()),
                        0, // mandatory
                        0, // immediate
                        &props,
                        amqp_bytes_malloc_dup(
                            amqp_cstring_bytes(
                                data.constData())));

                    if (status == AMQP_STATUS_OK)
                    {
                        m_lastHeartbeatSent =
                            QDateTime::currentDateTime()
                                .toMSecsSinceEpoch();
                        qDebug() << "Heartbeat sent "
                                    "successfully";
                    }
                    else
                    {
                        qWarning()
                            << "Failed to send heartbeat: "
                            << status;
                    }
                }
                catch (const std::exception &e)
                {
                    qWarning()
                        << "Exception during heartbeat:"
                        << e.what();
                }
            }

            // Sleep for the specified interval
            for (int i = 0;
                 i < interval * 2 && m_heartbeatActive; ++i)
            {
                QThread::msleep(
                    500); // Check termination frequently
            }
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception in heartbeat thread:"
                   << e.what();
    }

    qDebug() << "Heartbeat thread terminating";
}

/**
 * Sets up a heartbeat mechanism to maintain connection
 * health.
 *
 * @param heartbeatInterval Interval in seconds between
 * heartbeats
 */
void RabbitMQHandler::setupHeartbeat(int heartbeatInterval)
{
    QMutexLocker locker(&m_mutex);

    if (m_heartbeatActive)
    {
        qDebug() << "Heartbeat already active";
        return;
    }

    m_heartbeatActive = true;
    m_lastHeartbeatSent =
        QDateTime::currentDateTime().toMSecsSinceEpoch();

    // Start heartbeat thread
    m_heartbeatThread      = new QThread();
    QTimer *heartbeatTimer = new QTimer(nullptr);
    heartbeatTimer->moveToThread(m_heartbeatThread);
    heartbeatTimer->setInterval(heartbeatInterval * 1000);

    connect(
        m_heartbeatThread, &QThread::started,
        heartbeatTimer,
        static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(heartbeatTimer, &QTimer::timeout, this,
            [this, heartbeatInterval]() {
                this->heartbeatThreadFunction(
                    heartbeatInterval);
            });
    connect(m_heartbeatThread, &QThread::finished,
            heartbeatTimer, &QTimer::stop);
    connect(m_heartbeatThread, &QThread::finished,
            heartbeatTimer, &QTimer::deleteLater);

    m_heartbeatThread->start();

    qDebug() << "Heartbeat mechanism started with interval"
             << heartbeatInterval << "seconds";
}

/**
 * Stops the heartbeat mechanism.
 */
void RabbitMQHandler::stopHeartbeat()
{
    QMutexLocker locker(&m_mutex);

    if (!m_heartbeatActive)
    {
        return;
    }

    m_heartbeatActive = false;

    if (m_heartbeatThread)
    {
        if (m_heartbeatThread->isRunning())
        {
            m_heartbeatThread->quit();
            m_heartbeatThread->wait(
                2000); // Wait up to 2 seconds

            if (m_heartbeatThread->isRunning())
            {
                qWarning()
                    << "Heartbeat thread didn't exit, "
                    << "terminating";
                m_heartbeatThread->terminate();
            }
        }

        delete m_heartbeatThread;
        m_heartbeatThread = nullptr;
    }

    qDebug() << "Heartbeat mechanism stopped";
}

/**
 * Reconnects the sending connection if it was lost.
 */
void RabbitMQHandler::reconnectSending()
{
    qDebug()
        << "Attempting to reconnect sending connection";

    // Close existing connection if present
    if (m_sendConnection)
    {
        try
        {
            amqp_channel_close(m_sendConnection, 1,
                               AMQP_REPLY_SUCCESS);
            amqp_connection_close(m_sendConnection,
                                  AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(m_sendConnection);
        }
        catch (const std::exception &e)
        {
            qWarning()
                << "Error closing previous connection:"
                << e.what();
        }
        m_sendConnection = nullptr;
    }

    // Create new connection
    m_sendConnection = amqp_new_connection();
    if (!m_sendConnection)
    {
        qWarning()
            << "Failed to create new send connection";
        return;
    }

    // Create socket
    amqp_socket_t *socket =
        amqp_tcp_socket_new(m_sendConnection);
    if (!socket)
    {
        qWarning() << "Failed to create new send socket";
        amqp_destroy_connection(m_sendConnection);
        m_sendConnection = nullptr;
        return;
    }

    // Open socket
    int status = amqp_socket_open(
        socket, m_host.toUtf8().constData(), m_port);
    if (status != AMQP_STATUS_OK)
    {
        qWarning() << "Failed to open new send socket: "
                   << status;
        amqp_destroy_connection(m_sendConnection);
        m_sendConnection = nullptr;
        return;
    }

    // Login
    amqp_rpc_reply_t loginReply =
        amqp_login(m_sendConnection,
                   "/",    // vhost
                   0,      // channel max
                   131072, // frame max
                   0,      // heartbeat
                   AMQP_SASL_METHOD_PLAIN,
                   m_username.toUtf8().constData(),
                   m_password.toUtf8().constData());

    if (loginReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning()
            << "Failed to login to new send connection";
        amqp_destroy_connection(m_sendConnection);
        m_sendConnection = nullptr;
        return;
    }

    // Open channel
    amqp_channel_open(m_sendConnection, 1);
    amqp_rpc_reply_t channelReply =
        amqp_get_rpc_reply(m_sendConnection);
    if (channelReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning() << "Failed to open new send channel";
        amqp_destroy_connection(m_sendConnection);
        m_sendConnection = nullptr;
        return;
    }

    // Declare exchange
    amqp_exchange_declare(
        m_sendConnection,
        1, // channel
        amqp_cstring_bytes(m_exchange.toUtf8().constData()),
        amqp_cstring_bytes("topic"),
        0, // passive
        1, // durable
        0, // auto delete
        0, // internal
        amqp_empty_table);

    amqp_rpc_reply_t exchangeReply =
        amqp_get_rpc_reply(m_sendConnection);
    if (exchangeReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning() << "Failed to declare exchange for send";
        amqp_destroy_connection(m_sendConnection);
        m_sendConnection = nullptr;
        return;
    }

    qDebug()
        << "Successfully reconnected sending connection";
}

/**
 * Reconnects the receiving connection if it was lost.
 */
void RabbitMQHandler::reconnectReceiving()
{
    qDebug()
        << "Attempting to reconnect receiving connection";

    // Close existing connection if present
    if (m_receiveConnection)
    {
        try
        {
            amqp_channel_close(m_receiveConnection, 1,
                               AMQP_REPLY_SUCCESS);
            amqp_connection_close(m_receiveConnection,
                                  AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(m_receiveConnection);
        }
        catch (const std::exception &e)
        {
            qWarning()
                << "Error closing previous connection:"
                << e.what();
        }
        m_receiveConnection = nullptr;
    }

    // Create new connection
    m_receiveConnection = amqp_new_connection();
    if (!m_receiveConnection)
    {
        qWarning()
            << "Failed to create new receive connection";
        return;
    }

    // Create socket
    amqp_socket_t *socket =
        amqp_tcp_socket_new(m_receiveConnection);
    if (!socket)
    {
        qWarning() << "Failed to create new receive socket";
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Open socket
    int status = amqp_socket_open(
        socket, m_host.toUtf8().constData(), m_port);
    if (status != AMQP_STATUS_OK)
    {
        qWarning() << "Failed to open new receive socket: "
                   << status;
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Login
    amqp_rpc_reply_t loginReply =
        amqp_login(m_receiveConnection,
                   "/",    // vhost
                   0,      // channel max
                   131072, // frame max
                   0,      // heartbeat
                   AMQP_SASL_METHOD_PLAIN,
                   m_username.toUtf8().constData(),
                   m_password.toUtf8().constData());

    if (loginReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning()
            << "Failed to login to new receive connection";
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Open channel
    amqp_channel_open(m_receiveConnection, 1);
    amqp_rpc_reply_t channelReply =
        amqp_get_rpc_reply(m_receiveConnection);
    if (channelReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning() << "Failed to open new receive channel";
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Declare exchange
    amqp_exchange_declare(
        m_receiveConnection,
        1, // channel
        amqp_cstring_bytes(m_exchange.toUtf8().constData()),
        amqp_cstring_bytes("topic"),
        0, // passive
        1, // durable
        0, // auto delete
        0, // internal
        amqp_empty_table);

    amqp_rpc_reply_t exchangeReply =
        amqp_get_rpc_reply(m_receiveConnection);
    if (exchangeReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning()
            << "Failed to declare exchange for receive";
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Declare queue
    amqp_queue_declare(
        m_receiveConnection,
        1, // channel
        amqp_cstring_bytes(
            m_responseQueue.toUtf8().constData()),
        0, // passive
        1, // durable
        0, // exclusive
        0, // auto delete
        amqp_empty_table);

    amqp_rpc_reply_t queueReply =
        amqp_get_rpc_reply(m_receiveConnection);
    if (queueReply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        qWarning() << "Failed to declare queue for receive";
        amqp_destroy_connection(m_receiveConnection);
        m_receiveConnection = nullptr;
        return;
    }

    // Bind queue to each routing key
    for (const QString &routingKey : m_receivingRoutingKeys)
    {
        amqp_queue_bind(
            m_receiveConnection,
            1, // channel
            amqp_cstring_bytes(
                m_responseQueue.toUtf8().constData()),
            amqp_cstring_bytes(
                m_exchange.toUtf8().constData()),
            amqp_cstring_bytes(
                routingKey.toUtf8().constData()),
            amqp_empty_table);

        amqp_rpc_reply_t bindReply =
            amqp_get_rpc_reply(m_receiveConnection);
        if (bindReply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to bind queue with key: "
                       << routingKey;
            amqp_destroy_connection(m_receiveConnection);
            m_receiveConnection = nullptr;
            return;
        }
    }

    // Start consuming
    startConsuming();

    qDebug()
        << "Successfully reconnected receiving connection";
}

bool RabbitMQHandler::hasConsumers(const QString &queueName)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected || !m_sendConnection)
    {
        qWarning()
            << "Cannot check consumers: not connected";
        return false;
    }

    try
    {

        // Get queue info - use amqp_queue_declare_ok_t
        // directly
        amqp_queue_declare_ok_t *queueDeclareOk =
            amqp_queue_declare(
                m_sendConnection,
                1, // channel
                amqp_cstring_bytes(
                    queueName.toUtf8().constData()),
                1, // passive - don't create if doesn't
                   // exist
                1, // durable
                0, // exclusive
                0, // auto delete
                amqp_empty_table);

        // Check RPC reply
        amqp_rpc_reply_t reply =
            amqp_get_rpc_reply(m_sendConnection);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            qWarning() << "Failed to get queue info for"
                       << queueName
                       << "reply type:" << reply.reply_type;

            if (reply.reply_type
                == AMQP_RESPONSE_SERVER_EXCEPTION)
            {
                // This means the queue doesn't exist or
                // another server error
                if (reply.reply.id != 0)
                {
                    qWarning()
                        << "Server exception: class id ="
                        << (int)reply.reply.id;
                }
            }

            return false;
        }

        if (!queueDeclareOk)
        {
            qWarning() << "Queue declare OK is null for"
                       << queueName;
            return false;
        }

        // Log the consumer count
        int consumerCount = queueDeclareOk->consumer_count;
        qDebug() << "Queue" << queueName << "has"
                 << consumerCount << "consumers";

        // Log message count too
        int messageCount = queueDeclareOk->message_count;
        qDebug() << "Queue" << queueName << "has"
                 << messageCount << "messages";

        return consumerCount > 0;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception while checking consumers "
                      "for queue"
                   << queueName << ":" << e.what();
        return false;
    }
}

bool RabbitMQHandler::hasCommandQueueConsumers()
{
    bool result = hasConsumers(m_commandQueue);
    return result;
}

bool RabbitMQHandler::hasResponseQueueConsumers()
{
    return hasConsumers(m_responseQueue);
}

} // namespace Backend
} // namespace CargoNetSim
