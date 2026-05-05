#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{

class Path;

namespace Scenario
{

class ScenarioDocument;

/**
 * @brief Shared OD-demand resolver for discovered path previews.
 *
 * Path discovery returns ranked alternatives per origin/destination
 * pair. Those alternatives are comparison candidates and should all
 * see the same authored OD demand preview, independent of the later
 * rank-0-only dispatch allocation policy.
 */
namespace PathDemandResolver
{
    /**
     * @brief Resolve authored OD demand for an origin/destination pair.
     *
     * Returns `round(originContainerCount(originId) * destinationFraction)`.
     * Falls back to the full origin pool when the destination is not in the
     * authored routing list. Returns 0 when the origin has no seeded demand.
     */
    int previewContainerCount(
        const ScenarioDocument &document,
        const QString          &originId,
        const QString          &destinationId);

    /**
     * @brief Resolve authored OD demand for @p path's endpoints.
     *
     * Uses `Path::getOriginId()` / `getDestinationId()` when available and
     * falls back to segment endpoints otherwise.
     */
    int previewContainerCount(
        const ScenarioDocument &document,
        const CargoNetSim::Backend::Path &path);
} // namespace PathDemandResolver

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
