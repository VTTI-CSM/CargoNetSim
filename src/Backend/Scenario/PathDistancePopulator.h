// src/Backend/Scenario/PathDistancePopulator.h
#pragma once

#include <QList>

namespace CargoNetSim {
namespace Backend {

class Path;
class NetworkController;
class ConfigController;
class RegionDataController;

namespace Scenario {

class ScenarioDocument;
class ScenarioRegistry;

/// Populate PathSegment::estimated.{distance, duration} for every
/// segment of the given paths by dispatching per segment mode to
/// the appropriate mode module.
///
/// Stateless, pure-ish (reads from controllers via their public
/// APIs; writes only to segment attributes). Safe to call multiple
/// times — a second call overwrites the prior estimate.
namespace PathDistancePopulator
{
    /// Populate for every segment of every path. Paths whose
    /// segments reference terminals with no matching network
    /// linkage (e.g. ship segments) are populated via
    /// GeoDistance + config-average-speed. Returns the number of
    /// segments populated successfully.
    int populate(
        const QList<Path *>                &paths,
        const ScenarioDocument             &doc,
        const NetworkController            &networks,
        const ConfigController             &config,
        const RegionDataController         *regionData);
} // namespace PathDistancePopulator

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
