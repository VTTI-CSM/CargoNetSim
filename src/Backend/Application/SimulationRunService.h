#pragma once

#include <QString>
#include <QVector>

#include "Backend/Scenario/ExecutionPlanTypes.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioRuntime;
}

namespace Application
{

enum class SimulationRunServiceStatus
{
    Success,
    InvalidSelection,
    ValidationFailed,
    StartFailed
};

struct SimulationRunServiceResult
{
    SimulationRunServiceStatus status =
        SimulationRunServiceStatus::StartFailed;
    QString          message;
    QVector<QString> selectedPathKeys;
    int              selectedPathCount = 0;

    bool succeeded() const
    {
        return status == SimulationRunServiceStatus::Success;
    }
};

class SimulationRunService
{
public:
    SimulationRunService() = default;

    SimulationRunServiceResult selectAndValidate(
        Scenario::ScenarioRuntime &runtime,
        const QVector<QString>    &selectedPathKeys,
        Scenario::ExecutionDemandPolicy demandPolicy) const;

    SimulationRunServiceResult selectAndValidate(
        Scenario::ScenarioRuntime &runtime,
        const QVector<QString>    &selectedPathKeys) const;

    SimulationRunServiceResult validateAndStart(
        Scenario::ScenarioRuntime &runtime) const;

    SimulationRunServiceResult selectValidateAndStart(
        Scenario::ScenarioRuntime &runtime,
        const QVector<QString>    &selectedPathKeys,
        Scenario::ExecutionDemandPolicy demandPolicy) const;

    SimulationRunServiceResult selectValidateAndStart(
        Scenario::ScenarioRuntime &runtime,
        const QVector<QString>    &selectedPathKeys) const;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
