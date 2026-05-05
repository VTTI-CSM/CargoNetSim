// src/Backend/Scenario/PathMetricsCalculator.h
#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "PathMetrics.h"
#include "PathMetricsInputs.h"
#include "SimulationSettings.h"

namespace CargoNetSim {
namespace Backend {

class ConfigController;

namespace Scenario {

/// Free-function namespace for per-path metric computation.
/// Pure by construction: no state, no singletons, no services
/// in the `compute` signature.
namespace PathMetricsCalculator
{
    /// Pure function: data in, data out. No controllers, no globals,
    /// no scene access. Testable with hand-built `PathMetricsInputs`.
    ///
    /// `mode == TransportationMode::Any` (or an unknown value)
    /// yields `valid == false`.
    PathMetrics compute(
        double                                           distanceMeters,
        double                                           timeSeconds,
        TransportationTypes::TransportationMode          mode,
        const PathMetricsInputs                         &inputs);

    /// Container-aware overload — calls 4-arg compute, then
    /// projectPerContainer on top using capacity from modeProperties.
    PathMetrics compute(
        double                                   distanceMeters,
        double                                   timeSeconds,
        TransportationTypes::TransportationMode  mode,
        const PathMetricsInputs                 &inputs,
        int                                      containerCount);

    /// Per-container projection. In-place. No-op if !m.valid or
    /// containerCount <= 0. Shared by the 5-arg compute overload AND
    /// actuals handling in GUI completion (which never runs through
    /// compute — simulator measurements are authoritative).
    void projectPerContainer(PathMetrics &m,
                             int          containerCount,
                             int          capacity);

    /// The one place that reaches into ConfigController. GUI authoring,
    /// prediction, and reconciliation code all call the same adapter to
    /// build `PathMetricsInputs` — no duplication of config-access code.
    PathMetricsInputs gatherInputs(
        TransportationTypes::TransportationMode          mode,
        const ConfigController                          &config);

    /// Same adapter as above, but with scenario YAML overrides applied on top
    /// of config.xml defaults without mutating ConfigController. Use this for
    /// headless/preview flows that need route metrics before ScenarioApplier
    /// commits the scenario settings into the live controller.
    PathMetricsInputs gatherInputs(
        TransportationTypes::TransportationMode          mode,
        const ConfigController                          &config,
        const SimulationSettings                        &settings);
} // namespace PathMetricsCalculator

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
