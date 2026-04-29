#include "TerminalInventoryGateway.h"

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

TerminalSimulationInventoryGateway::TerminalSimulationInventoryGateway(
    TerminalSimulationClient *client)
    : m_client(client)
{
}

bool TerminalSimulationInventoryGateway::addContainers(
    const QString                     &terminalId,
    QList<ContainerCore::Container *> &containers,
    double                             addTimeSeconds,
    const QString                     &arrivalMode)
{
    return m_client
        && m_client->addContainers(terminalId,
                                   containers,
                                   addTimeSeconds,
                                   arrivalMode);
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
    const QString &reservationId)
{
    return m_client
        ? m_client->commitContainerReservation(terminalId,
                                               reservationId)
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
