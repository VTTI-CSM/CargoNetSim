#pragma once

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Per-path cost breakdown emitted by ScenarioExecutor.
 *
 * The executor emits one of these per path it simulates. Segment-level
 * writeback (actual_values / actual_cost) happens directly on the Path's
 * segments via PathSegment::setAttributes — this struct carries only the
 * aggregate summary the consumer (GUI table / CLI JSON writer) needs.
 */
struct PathSimulationResult
{
    int    pathId        = -1;
    double totalCost     = 0.0;
    double edgeCosts     = 0.0;
    double terminalCosts = 0.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
