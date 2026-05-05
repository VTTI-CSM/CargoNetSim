#include "SimulatorHealthProbeTransport.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QMetaObject>
#include <QThread>
#include <QUuid>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/ThreadSafetyUtils.h"

namespace CargoNetSim
{
namespace Backend
{

SimulatorHealthProbeTransport::SimulatorHealthProbeTransport(
    QObject *parent, const QString &host, int port,
    const QString &username, const QString &password,
    const QString &exchange, const QString &commandQueue,
    const QString &responseQueue,
    const QString &sendingRoutingKey,
    const QStringList &receivingRoutingKeys,
    ClientType clientType)
    : QObject(parent)
    , m_rabbitMQHandler(new RabbitMQHandler(
          nullptr, host, port, username, password, exchange,
          commandQueue, responseQueue, sendingRoutingKey,
          receivingRoutingKeys))
    , m_clientType(clientType)
{
    connect(m_rabbitMQHandler, &RabbitMQHandler::messageReceived, this,
            &SimulatorHealthProbeTransport::handleMessage,
            // Health probes are a low-level synchronization path, not
            // a UI/event-stream path. Delivering directly here keeps the
            // response independent from whichever thread is currently
            // blocked waiting on the probe result.
            Qt::DirectConnection);
}

SimulatorHealthProbeTransport::~SimulatorHealthProbeTransport()
{
    disconnectFromServer();
    delete m_rabbitMQHandler;
    m_rabbitMQHandler = nullptr;
}

bool SimulatorHealthProbeTransport::connectToServer()
{
    if (QThread::currentThread() == thread())
        return connectToServerImpl();

    bool result = false;
    QMetaObject::invokeMethod(
        this, [this, &result]() {
            result = connectToServerImpl();
        },
        Qt::BlockingQueuedConnection);
    return result;
}

void SimulatorHealthProbeTransport::disconnectFromServer()
{
    if (QThread::currentThread() == thread())
    {
        disconnectFromServerImpl();
        return;
    }

    QMetaObject::invokeMethod(
        this, [this]() { disconnectFromServerImpl(); },
        Qt::BlockingQueuedConnection);
}

bool SimulatorHealthProbeTransport::isConnected() const
{
    if (QThread::currentThread() == thread())
        return isConnectedImpl();

    bool connected = false;
    QMetaObject::invokeMethod(
        const_cast<SimulatorHealthProbeTransport *>(this),
        [this, &connected]() {
            connected = isConnectedImpl();
        },
        Qt::BlockingQueuedConnection);
    return connected;
}

bool SimulatorHealthProbeTransport::probe(
    const QString &command, const QStringList &expectedEvents,
    int timeoutMs)
{
    if (QThread::currentThread() == thread())
        return probeImpl(command, expectedEvents, timeoutMs);

    bool result = false;
    QMetaObject::invokeMethod(
        this,
        [this, &result, command, expectedEvents, timeoutMs]() {
            result = probeImpl(command, expectedEvents, timeoutMs);
        },
        Qt::BlockingQueuedConnection);
    return result;
}

void SimulatorHealthProbeTransport::handleMessage(
    const QJsonObject &message)
{
    if (!message.contains(QStringLiteral("event")))
        return;

    const QString normalizedEvent = normalizeEventName(
        message.value(QStringLiteral("event")).toString());
    registerEvent(normalizedEvent, message);
}

bool SimulatorHealthProbeTransport::connectToServerImpl()
{
    if (m_rabbitMQHandler == nullptr)
        return false;
    return m_rabbitMQHandler->establishConnection();
}

void SimulatorHealthProbeTransport::disconnectFromServerImpl()
{
    if (m_rabbitMQHandler != nullptr)
        m_rabbitMQHandler->disconnect();
}

bool SimulatorHealthProbeTransport::isConnectedImpl() const
{
    return m_rabbitMQHandler != nullptr
           && m_rabbitMQHandler->isConnected();
}

bool SimulatorHealthProbeTransport::probeImpl(
    const QString &command, const QStringList &expectedEvents,
    int timeoutMs)
{
    if (!isConnectedImpl() && !connectToServerImpl())
        return false;
    return sendCommandAndWait(command, expectedEvents, timeoutMs);
}

bool SimulatorHealthProbeTransport::sendCommandAndWait(
    const QString &command, const QStringList &expectedEvents,
    int timeoutMs)
{
    if (expectedEvents.isEmpty())
        return false;

    {
        CargoNetSim::Backend::Commons::ScopedWriteLock locker(
            m_eventMutex);
        for (const QString &event : expectedEvents)
        {
            m_receivedEvents.remove(normalizeEventName(event));
        }
    }

    if (!sendCommand(command))
        return false;

    return waitForEvent(expectedEvents, timeoutMs);
}

bool SimulatorHealthProbeTransport::waitForEvent(
    const QStringList &expectedEvents, int timeoutMs)
{
    if (expectedEvents.isEmpty())
        return false;

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
        {
            CargoNetSim::Backend::Commons::ScopedReadLock readLocker(
                m_eventMutex);
            for (const QString &event : normalizedEvents)
            {
                if (m_receivedEvents.contains(event))
                {
                    receivedEvent = true;
                    break;
                }
            }
        }

        if (!receivedEvent)
        {
            CargoNetSim::Backend::Commons::ScopedWriteLock writeLocker(
                m_eventMutex);
            for (const QString &event : normalizedEvents)
            {
                if (m_receivedEvents.contains(event))
                {
                    receivedEvent = true;
                    break;
                }
            }

            if (!receivedEvent)
            {
                const int remainingTime =
                    timeoutMs - static_cast<int>(timer.elapsed());
                if (remainingTime <= 0)
                    return false;

                m_eventCondition.wait(&m_eventMutex, remainingTime);
            }
        }
    }

    return true;
}

bool SimulatorHealthProbeTransport::sendCommand(
    const QString &command)
{
    if (m_rabbitMQHandler == nullptr)
        return false;

    return m_rabbitMQHandler->sendCommand(createCommandObject(command));
}

QJsonObject SimulatorHealthProbeTransport::createCommandObject(
    const QString &command) const
{
    QJsonObject commandObj;
    commandObj[QStringLiteral("command")] = command;
    commandObj[QStringLiteral("timestamp")] =
        QDateTime::currentDateTime().toString(Qt::ISODate);
    commandObj[QStringLiteral("clientType")] =
        static_cast<int>(m_clientType);
    commandObj[QStringLiteral("commandId")] =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    return commandObj;
}

void SimulatorHealthProbeTransport::registerEvent(
    const QString &eventName, const QJsonObject &eventData)
{
    CargoNetSim::Backend::Commons::ScopedWriteLock locker(
        m_eventMutex);
    m_receivedEvents[eventName] = eventData;
    m_eventCondition.wakeAll();
}

QString SimulatorHealthProbeTransport::normalizeEventName(
    const QString &eventName)
{
    return eventName.trimmed().toLower().remove(' ');
}

} // namespace Backend
} // namespace CargoNetSim
