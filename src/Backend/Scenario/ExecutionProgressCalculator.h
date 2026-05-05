#pragma once

#include "ExecutionPlanTypes.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Builds user-facing execution progress from the canonical
 *        execution plan and ledger.
 *
 * Progress is based on CargoNetSim execution state, not simulator
 * wall-clock or simulator-local time. That keeps progress meaningful
 * for unbounded runs that stop when all selected executable paths are
 * complete.
 */
ExecutionProgressSnapshot calculateExecutionProgress(
    const ScenarioExecutionPlan &plan,
    const ExecutionLedger       &ledger);

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim

