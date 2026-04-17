// src/Backend/Scenario/PathMetrics.h
#pragma once

#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Output value of PathMetricsCalculator::compute.
///
/// Per-vehicle quantities. Per-container projections are added
/// by Plan 10's container-aware overload once ContainerAllocator
/// establishes how many containers flow through each path.
///
/// Units:
///   distanceKm         — km
///   travelTimeHours    — hours
///   riskPerVehicle     — risk-factor units (same scale as
///                        modeProperties["risk_factor"])
///   fuelPerVehicle     — fuel units per vehicle per trip
///   energyPerVehicle   — kWh per vehicle per trip
///   carbonPerVehicle   — tonnes CO₂ per vehicle per trip
struct PathMetrics
{
    bool    valid             = false;
    double  distanceKm        = 0.0;
    double  travelTimeHours   = 0.0;
    double  riskPerVehicle    = 0.0;
    double  fuelPerVehicle    = 0.0;
    double  energyPerVehicle  = 0.0;
    double  carbonPerVehicle  = 0.0;
    QString fuelType;  // human-facing label (e.g. "diesel_1")

    /// Number of containers assigned to this path by the allocator.
    /// Zero when no allocator has been consulted (e.g., a headless
    /// per-vehicle-only preview).
    int containerCount = 0;

    /// Per-container projections. These are populated by
    /// PathMetricsCalculator::compute's container-aware overload; the
    /// per-vehicle overload leaves them at zero. Arithmetic convention
    /// (correct direction this time): per-container = per-vehicle ×
    /// vehiclesNeeded / containerCount. A fuller vehicle dilutes each
    /// container's share; an almost-empty vehicle concentrates it.
    double fuelPerContainer    = 0.0;
    double energyPerContainer  = 0.0;
    double carbonPerContainer  = 0.0;
    double riskPerContainer    = 0.0;

    /// Vehicles the allocator's pool size requires at this mode's
    /// capacity (ceil division). Surfaced for display and as an
    /// intermediate product of the per-container math.
    int    vehiclesNeeded      = 0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
