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
/// touches ConfigController. Tests build this struct directly (no
/// ConfigController instance required); CLI and GUI both call the
/// adapter. One adapter, one compute function.
///
/// Expected map shapes (match ConfigController's getTransportModes,
/// getFuelEnergy, getFuelCarbonContent return types):
///   modeProperties    — { "average_fuel_consumption", "risk_factor",
///                         "use_network", "fuel_type", "average_speed",
///                         "average_locomotive_count", … }
///   fuelEnergy        — fuelType → calorific value (double)
///   fuelCarbonContent — fuelType → carbon content per unit (double)
struct PathMetricsInputs
{
    QVariantMap modeProperties;
    QVariantMap fuelEnergy;
    QVariantMap fuelCarbonContent;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
