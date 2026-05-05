#include "TerminalInventoryGateway.h"

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

QString terminalInventoryArrivalSemanticsToWire(
    TerminalInventoryArrivalSemantics semantics)
{
    switch (semantics)
    {
    case TerminalInventoryArrivalSemantics::RuntimeArrival:
        return QStringLiteral("runtime_arrival");
    case TerminalInventoryArrivalSemantics::Preload:
        return QStringLiteral("preload");
    case TerminalInventoryArrivalSemantics::Infer:
        return QString();
    }

    return QString();
}

TerminalSimulationInventoryGateway::TerminalSimulationInventoryGateway(
    TerminalSimulationClient *client)
    : m_client(client)
{
}

bool TerminalSimulationInventoryGateway::addContainers(
    const QString                     &terminalId,
    QList<ContainerCore::Container *> &containers,
    double                             addTimeSeconds,
    const QString                     &arrivalMode,
    TerminalInventoryArrivalSemantics  arrivalSemantics)
{
    return m_client
        && m_client->addContainers(terminalId,
                                   containers,
                                   addTimeSeconds,
                                   arrivalMode,
                                   terminalInventoryArrivalSemanticsToWire(
                                       arrivalSemantics));
}

QJsonObject TerminalSimulationInventoryGateway::reserveContainers(
    const QString     &terminalId,
    const QString     &reservationId,
    const QJsonObject &criteria)
{
    return m_client ? m_client->reserveContainers(terminalId,
                                                  reservationId,
                                                  criteria)
                    : QJsonObject{};
}

QJsonObject
TerminalSimulationInventoryGateway::commitContainerReservation(
    const QString &terminalId,
    const QString &reservationId,
    double operationTimeSeconds)
{
    return m_client
        ? m_client->commitContainerReservation(terminalId,
                                               reservationId,
                                               operationTimeSeconds)
        : QJsonObject{};
}

QJsonObject
TerminalSimulationInventoryGateway::releaseContainerReservation(
    const QString &terminalId,
    const QString &reservationId)
{
    return m_client
        ? m_client->releaseContainerReservation(terminalId,
                                                reservationId)
        : QJsonObject{};
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
