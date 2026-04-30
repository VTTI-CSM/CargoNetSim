#pragma once

#include <QVector>

#include "ExecutionPlanTypes.h"
#include "ScenarioRegistry.h"
#include "SegmentDispatchFactory.h"
#include "SimulationDispatchTypes.h"
#include "TerminalPickupCoordinator.h"

namespace CargoNetSim
{
namespace Backend
{
class ConfigController;
class VehicleController;

namespace Scenario
{

struct DispatchableWaveBuildResult
{
    SimulationRequestBundle bundle;
    QVector<VehicleDispatchAssignment> assignments;
    QVector<TerminalPickupReservationHandle> pickupReservations;
    bool terminalBlocked = false;
    QString terminalBlockedMessage;
    QString errorMessage;

    bool isSuccess() const
    {
        return errorMessage.isEmpty() && !terminalBlocked;
    }
};

class DispatchableWaveBuilder
{
public:
    DispatchableWaveBuilder(const ScenarioRegistry &registry,
                            ConfigController      *config,
                            VehicleController     *vehicles,
                            const QString         &executionId = QString(),
                            TerminalPickupCoordinator *pickupCoordinator = nullptr);

    DispatchableWaveBuildResult build(
        const ScenarioExecutionPlan           &plan,
        const ExecutionLedger                 &ledger,
        const QVector<DispatchableSegmentRef> &dispatchableSegments) const;

private:
    int capacityForMode(
        TransportationTypes::TransportationMode mode) const;

    const PathExecutionPlan *findPathPlan(
        const ScenarioExecutionPlan &plan,
        const QString               &executionPathKey) const;

    DispatchableWaveBuildResult fail(
        const QString &message) const;

    const ScenarioRegistry &m_registry;
    ConfigController       *m_config = nullptr;
    SegmentDispatchFactory  m_dispatchFactory;
    TerminalPickupCoordinator *m_pickupCoordinator = nullptr;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
