// src/Backend/Scenario/PathMetricsCalculator.h
#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "PathMetrics.h"
#include "PathMetricsInputs.h"

namespace CargoNetSim {
namespace Backend {

class ConfigController;
class VehicleController;

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

    /// The one place that reaches into controllers. GUI authoring,
    /// prediction, and reconciliation code all call the same adapter to
    /// build `PathMetricsInputs` — no duplication of
    /// controller-access code.
    PathMetricsInputs gatherInputs(
        TransportationTypes::TransportationMode          mode,
        const ConfigController                          &config,
        const VehicleController                         *vehicles);
} // namespace PathMetricsCalculator

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
