#pragma once

#include <QList>
#include <QMap>
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
 * @brief Per-mode segment cost math + segment-attribute writeback.
 *
 * This namespace exists for **separation of concerns**, not for
 * multi-consumer DRY. Same rationale as `TerminalCostMath`:
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
 * Active consumer: `ResultsExtractor`. Per-function bodies are extracted
 * verbatim from the pre-Plan-3 simulation driver; the only active
 * change was null-client guards at the top of each per-mode function
 * so they fail safely instead of crashing.
 */
namespace SegmentCostMath
{

/**
 * @brief Per-segment ship cost. Pulls ship state from @p shipClient,
 *        filters to ships belonging to (path.pathId, segmentCounter),
 *        aggregates BPR-style metrics, applies @p modeWeights, writes
 *        the per-metric actual_values and actual_cost back onto
 *        @p segment, and returns the segment cost scalar.
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
 * but is NOT used by the current SVW body (the per-trip loop is
 * preserved as a commented-out TODO in the impl). All metrics except
 * risk remain zero.
 *
 * The weight application uses different unit conversions than ship/train
 * (travelTime divided by 3600, distance divided by 1000 — legacy SVW
 * quirk that matters only once the commented-out loop is wired up).
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
 * @brief Accumulates @p details under `attributes[underlyingKey]` on
 *        @p segment. Existing keys are additively merged; new keys are
 *        inserted. Null segment is a no-op.
 */
void setActualDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QMap<QString, double>       &details,
    const QString                     &underlyingKey);

/**
 * @brief Removes `attributes[underlyingKey]` from @p segment (and all
 *        nested values). Null segment is a no-op. Missing key is a no-op.
 */
void deleteActualDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QString                     &underlyingKey);

/**
 * @brief Convenience: for every segment in @p path, removes the
 *        `actual_values` and `actual_cost` attribute groups. Used by
 *        both SVW and ResultsExtractor before recomputing per-path
 *        results from fresh simulator state.
 *
 * Null path is a no-op.
 */
void clearPathAttributes(CargoNetSim::Backend::Path *path);

/**
 * @brief Composes per-path costs into a single PathSimulationResult.
 *
 *   r.edgeCosts     = edgeCosts(...)             [SegmentCostMath]
 *   r.terminalCosts = totalTerminalCosts(...)    [TerminalCostMath]
 *   r.totalCost     = edgeCosts + terminalCosts
 *   r.pathId        = path->getPathId()
 *
 * Null path → default-constructed PathSimulationResult.
 */
CargoNetSim::Backend::Scenario::PathSimulationResult
computePathCosts(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount);

/**
 * @brief Computes the full typed execution result for a path without
 *        mutating PathSegment actual attributes.
 */
CargoNetSim::Backend::Scenario::PathExecutionResult
computePathExecutionResult(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QString                                             &pathIdentity,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount);

} // namespace SegmentCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
