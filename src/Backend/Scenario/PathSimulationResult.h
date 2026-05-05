#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Derived per-path cost summary for CLI/table/report consumers.
 *
 * A2's authoritative backend execution contract is
 * `ScenarioExecutionResultSet`. This struct is the compact summary view
 * projected from that richer result model for consumers that only need
 * aggregate path totals.
 */
struct PathSimulationResult
{
    int    pathId        = -1;
    QString canonicalPathKey;
    QString pathUid;
    QString originId;
    QString destinationId;
    int    rank          = 0;
    int    effectiveContainerCount = 0;
    double totalCost     = 0.0;
    double edgeCosts     = 0.0;
    double terminalCosts = 0.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
