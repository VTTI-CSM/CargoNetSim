// src/Backend/Scenario/PathMetrics.h
#pragma once

#include "Backend/Commons/TransportationMode.h"

#include <QList>
#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Output value of PathMetricsCalculator::compute.
///
/// Per-vehicle quantities. Per-container projections are added
/// by the container-aware overload once the caller chooses the
/// demand basis for this metrics snapshot.
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
    struct VehicleRequirement
    {
        int segmentIndex = -1;
        TransportationTypes::TransportationMode mode =
            TransportationTypes::TransportationMode::Any;
        int vehiclesNeeded = 0;
    };

    bool    valid             = false;
    double  distanceKm        = 0.0;
    double  travelTimeHours   = 0.0;
    double  riskPerVehicle    = 0.0;
    double  fuelPerVehicle    = 0.0;
    double  energyPerVehicle  = 0.0;
    double  carbonPerVehicle  = 0.0;
    QString fuelType;  // human-facing label (e.g. "diesel_1")

    /// Container count associated with this metrics snapshot.
    ///
    /// For predicted/discovery metrics this is the authored OD-demand
    /// preview for the path's origin/destination pair. For actual
    /// metrics this is the executed container count recorded by the
    /// runtime result.
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

    /// Legacy single-scalar vehicle count.
    ///
    /// This remains for compatibility with older snapshot / JSON
    /// consumers that expected one integer. For multimodal paths the
    /// authoritative preview is `previewVehicleBreakdown`.
    int    vehiclesNeeded      = 0;

    /// Authoritative per-segment preview vehicle requirements for the
    /// current metrics snapshot. For single-mode paths this will
    /// usually contain one repeated-mode entry per segment; for
    /// multimodal paths it preserves the segment order instead of
    /// collapsing the path onto the first segment's mode.
    QList<VehicleRequirement> previewVehicleBreakdown;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
