// src/Backend/Scenario/PathMetricsInputs.h
#pragma once

#include <QVariantMap>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Narrow data-only input to PathMetricsCalculator::compute.
///
/// Populated once per (scenario, mode) by
/// PathMetricsCalculator::gatherInputs — the single adapter that
/// touches controllers. Tests build this struct directly (no
/// ConfigController instance required); CLI and GUI both call the
/// adapter. One adapter, one compute function.
///
/// Expected map shapes (match ConfigController's getTransportModes,
/// getFuelEnergy, getFuelCarbonContent return types):
///   modeProperties    — { "average_fuel_consumption", "risk_factor",
///                         "use_network", "fuel_type",
///                         "average_speed", … }
///   fuelEnergy        — fuelType → calorific value (double)
///   fuelCarbonContent — fuelType → carbon content per unit (double)
struct PathMetricsInputs
{
    QVariantMap modeProperties;
    QVariantMap fuelEnergy;
    QVariantMap fuelCarbonContent;

    /// 1.0 for Truck/Ship. For Train: locomotive count of the
    /// available train fleet (multiplies per-vehicle energy). When
    /// no train is loaded, defaults to 1.0 (no scaling).
    double locomotiveMultiplier = 1.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
