#pragma once

/**
 * @file EstimatedPhysicsPopulator.h
 * @brief Pipeline pass that populates estimated physical metrics on path segments.
 * @author Ahmed Aredah
 */

namespace CargoNetSim
{
namespace Backend
{

class Path;
class ConfigController;

namespace Scenario
{

class ScenarioDocument;

/**
 * @brief Pipeline pass that populates estimated physical metrics
 *        (energy consumption, carbon emissions, risk) on path segments.
 *
 * Position in the pipeline
 * ------------------------
 *   PathDiscovery            -> segments with mode, no metrics
 *   PathDistancePopulator    -> adds estimatedDistance + estimatedTravelTime
 *   EstimatedPhysicsPopulator -> adds estimatedEnergy, estimatedCarbon,
 *                               estimatedRisk                          <- here
 *   [simulation]             -> SegmentCostMath writes actual_values
 *
 * Responsibilities
 * ----------------
 * - Resolve the effective container count for each path from the
 *   ScenarioDocument (origin.initial_container_count x destination_fraction).
 * - Delegate all physics math to SegmentPhysicsEstimator::estimate().
 * - Write results via PathSegment::setEstimatedPhysicalMetrics().
 *
 * This class deliberately contains no fuel formulas. All physics math lives
 * in SegmentPhysicsEstimator and is independently unit-testable.
 *
 * Precondition
 * ------------
 * PathDistancePopulator must have run first; estimatedDistance() is required
 * input to SegmentPhysicsEstimator. Segments with estimatedDistance() == 0
 * are silently skipped (PathDistancePopulator failed on them).
 *
 * Geometry source
 * ---------------
 * For Rail/Truck, PathDistancePopulator uses Dijkstra on locally loaded
 * network graphs (real topology). For Ship, it uses haversine + config speed
 * (no ship network exists in CargoNetSim). TerminalSim is never involved in
 * distance, time, or physics estimation.
 */
class EstimatedPhysicsPopulator
{
public:
    /**
     * @param config  Merged config controller (config.xml + YAML overrides).
     *                Must not be null; must outlive this object.
     */
    explicit EstimatedPhysicsPopulator(ConfigController *config);

    /**
     * @brief Populate estimated physical metrics for all segments in @p path.
     *
     * @param path      Path whose segments are populated in-place. Skips
     *                  segments where estimatedDistance() == 0.
     * @param document  Scenario document used to resolve container count for
     *                  the path's origin to destination pair.
     */
    void populate(Path                   *path,
                  const ScenarioDocument &document) const;

private:
    /**
     * @brief Resolve effective container count for @p path.
     *
     * Returns round(origin.initial_container_count x destination_fraction),
     * where origin = path's first segment start terminal and destination =
     * path's last segment end terminal.
     *
     * Falls back to full initial_container_count when the destination is not
     * found in the origin's destinations list (conservative overestimate).
     * Returns 0 when the origin terminal has no initial_container_count.
     */
    int resolveContainerCount(const Path             *path,
                              const ScenarioDocument &document) const;

    ConfigController *m_config;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
