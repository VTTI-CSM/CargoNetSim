#pragma once

/**
 * @file SegmentPhysicsEstimator.h
 * @brief Pure static math service for pre-simulation segment physics estimation.
 * @author Ahmed Aredah
 */

#include "Backend/Commons/TransportationMode.h"
#include <QVariantMap>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Estimates pre-simulation energy consumption, carbon emissions, and
 *        risk for a single path segment using config-derived fuel parameters.
 *
 * This class is a pure stateless math service. It has no knowledge of the
 * simulation pipeline, network topology, or path structure. All inputs are
 * scalars; all outputs are collected in Result.
 *
 * Estimation model
 * ----------------
 * 1. vehicleCount  = ceil(containerCount / vehicleCapacity)
 * 2. fuelLitres    = fuelRate_L_per_km × (distanceMetres / 1000) × vehicleCount
 * 3. energyKWh     = fuelLitres × fuelEnergyKWhPerL
 * 4. carbonTonnes  = fuelLitres × fuelCarbonKgPerL / 1000
 * 5. risk          = riskFactor × vehicleCount
 *
 * Unit verification: `average_fuel_consumption` in config.xml is in L/km.
 * Confirmed from PathMetricsCalculator.cpp lines 102-123 which computes
 * energyPerVehicle = fuelRate × distanceKm × calorific.
 *
 * All fuel parameters are sourced from ConfigController (which already merges
 * config.xml defaults with any YAML scenario overrides), so YAML-defined
 * custom fuel types are automatically honoured.
 *
 * Units contract
 * --------------
 * Input  distanceMetres  : metres
 * Input  containerCount  : integer (≥ 0); returns zero Result when 0
 * Output energyKWh       : kWh
 * Output carbonTonnes    : tonnes CO₂
 * Output risk            : dimensionless (riskFactor × vehicleCount)
 *
 * Failure behaviour
 * -----------------
 * Returns all-zero Result and emits qCWarning when:
 *   - distanceMetres <= 0 or containerCount <= 0
 *   - fuel type is missing from fuelEnergy or fuelCarbonContent maps
 * Never throws.
 */
class SegmentPhysicsEstimator
{
public:
    struct Result {
        double energyKWh    = 0.0;
        double carbonTonnes = 0.0;
        double risk         = 0.0;
    };

    /**
     * @param mode              Transportation mode (Ship, Train, Truck).
     * @param distanceMetres    Estimated segment distance in metres.
     * @param containerCount    Containers assigned to this shipment leg (>= 0).
     * @param transportModes    ConfigController::getTransportModes() output.
     * @param fuelEnergy        ConfigController::getFuelEnergy() (kWh/L).
     * @param fuelCarbonContent ConfigController::getFuelCarbonContent() (kg CO2/L).
     * @return Estimated physics; all fields zero on missing/invalid input.
     */
    static Result estimate(
        TransportationTypes::TransportationMode mode,
        double                                  distanceMetres,
        int                                     containerCount,
        const QVariantMap                      &transportModes,
        const QVariantMap                      &fuelEnergy,
        const QVariantMap                      &fuelCarbonContent);

private:
    SegmentPhysicsEstimator() = delete;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
