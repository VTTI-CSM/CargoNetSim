#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QStringList>
#include <QWaitCondition>

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Commons/ClientType.h"

namespace CargoNetSim
{
namespace Backend
{

class SimulatorHealthProbeTransport : public QObject
{
    Q_OBJECT

public:
    explicit SimulatorHealthProbeTransport(
        QObject *parent, const QString &host, int port,
        const QString &username, const QString &password,
        const QString &exchange, const QString &commandQueue,
        const QString &responseQueue,
        const QString &sendingRoutingKey,
        const QStringList &receivingRoutingKeys,
        ClientType clientType);

    ~SimulatorHealthProbeTransport() override;

    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    bool probe(const QString &command,
               const QStringList &expectedEvents,
               int timeoutMs);

private slots:
    void handleMessage(const QJsonObject &message);

private:
    bool connectToServerImpl();
    void disconnectFromServerImpl();
    bool isConnectedImpl() const;
    bool probeImpl(const QString &command,
                   const QStringList &expectedEvents,
                   int timeoutMs);
    bool sendCommandAndWait(const QString &command,
                           const QStringList &expectedEvents,
                           int timeoutMs);
    bool waitForEvent(const QStringList &expectedEvents,
                      int timeoutMs);
    bool sendCommand(const QString &command);
    QJsonObject createCommandObject(const QString &command) const;
    void registerEvent(const QString     &eventName,
                       const QJsonObject &eventData);
    static QString normalizeEventName(const QString &eventName);

    RabbitMQHandler *m_rabbitMQHandler = nullptr;
    ClientType       m_clientType;

    QMap<QString, QJsonObject> m_receivedEvents;
    mutable QReadWriteLock     m_eventMutex;
    QWaitCondition             m_eventCondition;
};

} // namespace Backend
} // namespace CargoNetSim
