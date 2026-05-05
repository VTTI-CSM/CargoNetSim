#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace ContainerCore
{
class Container;
}

namespace CargoNetSim
{
namespace Backend
{

class TerminalSimulationClient;

namespace Scenario
{

enum class TerminalInventoryArrivalSemantics
{
    Infer,
    RuntimeArrival,
    Preload
};

QString terminalInventoryArrivalSemanticsToWire(
    TerminalInventoryArrivalSemantics semantics);

class TerminalInventoryGateway
{
public:
    virtual ~TerminalInventoryGateway() = default;

    virtual bool addContainers(
        const QString                     &terminalId,
        QList<ContainerCore::Container *> &containers,
        double                             addTimeSeconds,
        const QString                     &arrivalMode,
        TerminalInventoryArrivalSemantics arrivalSemantics =
            TerminalInventoryArrivalSemantics::Infer) = 0;

    virtual QJsonObject reserveContainers(
        const QString     &terminalId,
        const QString     &reservationId,
        const QJsonObject &criteria) = 0;

    virtual QJsonObject commitContainerReservation(
        const QString &terminalId,
        const QString &reservationId,
        double operationTimeSeconds) = 0;

    virtual QJsonObject releaseContainerReservation(
        const QString &terminalId,
        const QString &reservationId) = 0;
};

class TerminalSimulationInventoryGateway final
    : public TerminalInventoryGateway
{
public:
    explicit TerminalSimulationInventoryGateway(
        TerminalSimulationClient *client);

    bool addContainers(
        const QString                     &terminalId,
        QList<ContainerCore::Container *> &containers,
        double                             addTimeSeconds,
        const QString                     &arrivalMode,
        TerminalInventoryArrivalSemantics arrivalSemantics =
            TerminalInventoryArrivalSemantics::Infer) override;

    QJsonObject reserveContainers(
        const QString     &terminalId,
        const QString     &reservationId,
        const QJsonObject &criteria) override;

    QJsonObject commitContainerReservation(
        const QString &terminalId,
        const QString &reservationId,
        double operationTimeSeconds) override;

    QJsonObject releaseContainerReservation(
        const QString &terminalId,
        const QString &reservationId) override;

private:
    TerminalSimulationClient *m_client = nullptr;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
