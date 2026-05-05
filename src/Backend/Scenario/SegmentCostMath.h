#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include "ScenarioExecutionResult.h"
#include "PathSimulationResult.h"

namespace CargoNetSim
{
namespace Backend
{
class Path;
class PathSegment;
namespace ShipClient
{
class ShipSimulationClient;
}
namespace TrainClient
{
class TrainSimulationClient;
}
namespace TruckClient
{
class TruckSimulationManager;
}
namespace Scenario
{

/**
 * @brief Per-mode segment cost math for typed execution results.
 *
 * This namespace exists for **separation of concerns**, not for
 * multi-consumer DRY:
 *
 *   - **Testability.** Per-mode scalar math stays free of orchestration
 *     coupling; inputs are client pointers and plain structs, so tests
 *     can feed in minimal fixtures without a live runtime.
 *   - **Readability.** `ResultsExtractor` reads as a linear pipeline
 *     (iterate paths → dispatch by segment mode → aggregate). The
 *     ~500 lines of per-mode math would otherwise overwhelm the
 *     orchestration code.
 *   - **Future reuse.** CLI analyzers, report generators, or offline
 *     cost recomputation can consume these directly without pulling in
 *     the full ResultsExtractor pipeline.
 *
 * Stateless — no QObject, no cached state. Simulation clients are
 * passed as raw non-owning pointers per call so the functions stay
 * pure of object-graph ownership concerns.
 *
 * Active consumer: `ResultsExtractor`. Segment actuals are returned through
 * `ScenarioExecutionResult`; prepared `PathSegment` objects are not mutated.
 */
namespace SegmentCostMath
{

/**
 * @brief Per-segment ship cost. Pulls ship state from @p shipClient,
 *        filters to ships belonging to (path.pathId, segmentCounter),
 *        aggregates BPR-style metrics, applies @p modeWeights, and returns
 *        the segment cost scalar without mutating @p segment.
 *
 * Guards: returns 0.0 if any of @p shipClient, @p path, @p segment are
 * null, or if no matching ships are found.
 */
double shipSegmentCost(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient *shipClient,
    CargoNetSim::Backend::Path                             *path,
    CargoNetSim::Backend::PathSegment                      *segment,
    int                                                     segmentCounter,
    const QVariantMap                                      &modeWeights,
    const QVariantMap                                      &transportModes,
    int                                                     containerCount);

/**
 * @brief Per-segment train cost. Same structure as shipSegmentCost, but
 *        pulls state from @p trainClient->getAllTrainsStates(), reads
 *        `transportModes["rail"]`, and uses TrainState's getTrainUserId /
 *        getTotalCarbonDioxideEmitted / getTotalEnergyConsumed accessors.
 *
 * Guards: returns 0.0 if any of @p trainClient, @p path, @p segment are
 * null, or if no matching trains are found.
 */
double trainSegmentCost(
    CargoNetSim::Backend::TrainClient::TrainSimulationClient *trainClient,
    CargoNetSim::Backend::Path                               *path,
    CargoNetSim::Backend::PathSegment                        *segment,
    int                                                       segmentCounter,
    const QVariantMap                                        &modeWeights,
    const QVariantMap                                        &transportModes,
    int                                                       containerCount);

/**
 * @brief Per-segment truck cost — HEURISTIC path. Unlike ship/train,
 *        truck does not aggregate per-vehicle state; instead it derives
 *        truckCount from containerCount + capacity (ceil division) and
 *        applies the mode risk_factor.
 *
 * @p truckManager is accepted for signature symmetry with ship/train
 * but is not used until the truck simulator exposes per-trip actuals.
 * All metrics except risk remain zero in the current implementation.
 *
 * The shared typed-result path keeps metric units canonical
 * (seconds/metres/tonnes/kWh) for truck as well as ship/train.
 */
double truckSegmentCost(
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    CargoNetSim::Backend::PathSegment                         *segment,
    int                                                        segmentCounter,
    const QVariantMap                                         &modeWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount);

/**
 * @brief Mode-dispatch driver for a full path's edge costs.
 *
 * For each segment, selects mode-specific weights from
 * @p costFunctionWeights (keyed by the integer mode enum; falls back to
 * "default") and dispatches to shipSegmentCost / trainSegmentCost /
 * truckSegmentCost. Returns the summed edge cost.
 *
 * Empty segment list or any null segment entries → those segments
 * contribute 0.0. Null path is tolerated.
 */
double edgeCosts(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QList<CargoNetSim::Backend::PathSegment *>          &segments,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount);

/**
 * @brief Computes the full typed execution result for a path without
 *        mutating PathSegment attributes.
 */
CargoNetSim::Backend::Scenario::PathExecutionResult
computePathExecutionResult(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QString                                             &executionPathKey,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount,
    bool                                                       emitInfoLog = true);

} // namespace SegmentCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
