#pragma once

#include <QList>
#include <QVector>
#include <QString>

#include "ExecutionPlanTypes.h"
#include "PathAllocation.h"

namespace ContainerCore
{
class Container;
}

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

class TerminalInventoryGateway;

struct TerminalPickupRequest
{
    QString executionId;
    QString executionPathKey;
    QString canonicalPathKey;
    QString terminalId;
    int     segmentIndex = -1;
    int     containerCount = 0;
};

struct TerminalPickupReservationHandle
{
    QString terminalId;
    QString reservationId;
    QString executionPathKey;
    QString canonicalPathKey;
    int     segmentIndex = -1;
    int     containerCount = 0;

    bool isValid() const
    {
        return !terminalId.isEmpty()
            && !reservationId.isEmpty()
            && segmentIndex >= 0;
    }
};

struct TerminalPickupBatch
{
    TerminalPickupReservationHandle handle;
    QList<ContainerCore::Container *> containers;
    bool blocked = false;
    QString errorMessage;

    bool isSuccess() const
    {
        return errorMessage.isEmpty() && !blocked;
    }
};

class TerminalPickupCoordinator
{
public:
    explicit TerminalPickupCoordinator(
        TerminalInventoryGateway *gateway);

    bool seedExecutionInventory(
        const ScenarioExecutionPlan &plan,
        const PathAllocation        &allocation,
        double                       addTimeSeconds,
        QString                     *err) const;

    TerminalPickupBatch reserveForDispatch(
        const TerminalPickupRequest &request) const;

    bool commitReservations(
        const QVector<TerminalPickupReservationHandle> &handles,
        double operationTimeSeconds,
        QString                                        *err) const;

    bool releaseReservations(
        const QVector<TerminalPickupReservationHandle> &handles,
        QString                                        *err = nullptr) const;

private:
    TerminalPickupReservationHandle makeHandle(
        const TerminalPickupRequest &request) const;

    TerminalInventoryGateway *m_gateway = nullptr;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
