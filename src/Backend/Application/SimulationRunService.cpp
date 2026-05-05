#include "SimulationRunService.h"

#include <QObject>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/ScenarioRuntime.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

SimulationRunServiceResult SimulationRunService::selectAndValidate(
    Scenario::ScenarioRuntime &runtime,
    const QVector<QString>    &selectedPathKeys,
    Scenario::ExecutionDemandPolicy demandPolicy) const
{
    SimulationRunServiceResult result;
    result.selectedPathKeys = selectedPathKeys;

    if (selectedPathKeys.isEmpty())
    {
        result.status = SimulationRunServiceStatus::InvalidSelection;
        result.message =
            QStringLiteral("No paths selected for simulation");
        return result;
    }

    QString selectionError;
    if (!runtime.setSelectedPathKeys(selectedPathKeys, &selectionError))
    {
        result.status = SimulationRunServiceStatus::InvalidSelection;
        result.message = selectionError.isEmpty()
            ? QStringLiteral("Selected paths are no longer available")
            : selectionError;
        return result;
    }

    runtime.setDemandPolicy(demandPolicy);
    runtime.refreshPreparedPathEligibility();

    if (!runtime.validateCurrentSelectionForSimulation(&selectionError))
    {
        result.status = SimulationRunServiceStatus::ValidationFailed;
        result.message = selectionError.isEmpty()
            ? QStringLiteral("Selected paths cannot be simulated")
            : selectionError;
        return result;
    }

    result.status = SimulationRunServiceStatus::Success;
    result.selectedPathCount = runtime.paths().size();
    return result;
}

SimulationRunServiceResult SimulationRunService::selectAndValidate(
    Scenario::ScenarioRuntime &runtime,
    const QVector<QString>    &selectedPathKeys) const
{
    return selectAndValidate(
        runtime, selectedPathKeys,
        Scenario::ExecutionDemandPolicy::AllocatedOnly);
}

SimulationRunServiceResult SimulationRunService::validateAndStart(
    Scenario::ScenarioRuntime &runtime) const
{
    SimulationRunServiceResult result;
    result.selectedPathCount = runtime.paths().size();

    QString validationError;
    runtime.refreshPreparedPathEligibility();
    if (!runtime.validateCurrentSelectionForSimulation(&validationError))
    {
        result.status = SimulationRunServiceStatus::ValidationFailed;
        result.message = validationError.isEmpty()
            ? QStringLiteral("Selected paths cannot be simulated")
            : validationError;
        return result;
    }

    QString startError;
    const auto runtimeFailureConnection = QObject::connect(
        &runtime, &Scenario::ScenarioRuntime::failed,
        &runtime,
        [&startError](const QString &message) {
            startError = message;
        },
        Qt::DirectConnection);

    const bool started = runtime.startSimulation();
    QObject::disconnect(runtimeFailureConnection);

    if (!started)
    {
        result.status = SimulationRunServiceStatus::StartFailed;
        result.message = startError.isEmpty()
            ? QStringLiteral("Failed to start simulation")
            : startError;
        qCWarning(lcScenario)
            << "SimulationRunService::validateAndStart:"
            << result.message;
        return result;
    }

    result.status = SimulationRunServiceStatus::Success;
    return result;
}

SimulationRunServiceResult SimulationRunService::selectValidateAndStart(
    Scenario::ScenarioRuntime &runtime,
    const QVector<QString>    &selectedPathKeys,
    Scenario::ExecutionDemandPolicy demandPolicy) const
{
    auto result =
        selectAndValidate(runtime, selectedPathKeys, demandPolicy);
    if (!result.succeeded())
        return result;

    auto startResult = validateAndStart(runtime);
    startResult.selectedPathKeys = selectedPathKeys;
    if (startResult.selectedPathCount <= 0)
        startResult.selectedPathCount = result.selectedPathCount;
    return startResult;
}

SimulationRunServiceResult SimulationRunService::selectValidateAndStart(
    Scenario::ScenarioRuntime &runtime,
    const QVector<QString>    &selectedPathKeys) const
{
    return selectValidateAndStart(
        runtime, selectedPathKeys,
        Scenario::ExecutionDemandPolicy::AllocatedOnly);
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
