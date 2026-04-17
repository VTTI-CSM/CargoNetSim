// src/Backend/Scenario/PathMetricsCalculator.cpp
#include "PathMetricsCalculator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/TrainSystem.h"
#include "PropertyKeys.h"

#include <QString>
#include <algorithm>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

namespace PK = PropertyKeys;

namespace PathMetricsCalculator {

namespace {

struct ModeLookup
{
    const char *configKey;    // "rail" | "truck" | "ship"
    const char *defaultFuel;
    double      defaultSpeed; // km/h
};

bool modeSupported(TransportationTypes::TransportationMode m)
{
    using Mode = TransportationTypes::TransportationMode;
    bool supported = (m == Mode::Train || m == Mode::Truck || m == Mode::Ship);
    if (!supported)
        qCWarning(lcScenario) << "PathMetricsCalculator::modeSupported:"
                              << "unsupported mode" << static_cast<int>(m);
    return supported;
}

ModeLookup lookupFor(TransportationTypes::TransportationMode m)
{
    using Mode = TransportationTypes::TransportationMode;
    ModeLookup lk;
    switch (m)
    {
    case Mode::Train: lk = { "rail",  "diesel_1", 60.0 }; break;
    case Mode::Truck: lk = { "truck", "diesel_2", 60.0 }; break;
    case Mode::Ship:  lk = { "ship",  "HFO",      30.0 }; break;
    default:          lk = { "ship", "HFO", 30.0 }; break;  // unreachable
    }
    qCDebug(lcScenario) << "PathMetricsCalculator::lookupFor:"
                        << "mode=" << static_cast<int>(m)
                        << "configKey=" << lk.configKey;
    return lk;
}

} // namespace

PathMetrics compute(double                                     distanceMeters,
                    double                                     timeSeconds,
                    TransportationTypes::TransportationMode    mode,
                    const PathMetricsInputs                   &inputs)
{
    qCDebug(lcScenario) << "PathMetricsCalculator::compute:"
                        << "mode =" << static_cast<int>(mode)
                        << ", distance =" << distanceMeters << "m"
                        << ", time =" << timeSeconds << "s";

    PathMetrics out;
    if (!modeSupported(mode))       return out;
    if (inputs.modeProperties.isEmpty())
    {
        qCWarning(lcScenario) << "PathMetricsCalculator::compute:"
                              << "empty modeProperties for mode"
                              << static_cast<int>(mode);
        return out;
    }

    using Mode = TransportationTypes::TransportationMode;
    const ModeLookup lk = lookupFor(mode);

    const bool useNetwork =
        mode != Mode::Ship
        && inputs.modeProperties
               .value(PK::Mode::UseNetwork, false).toBool();

    out.distanceKm = distanceMeters / 1000.0;

    if (useNetwork)
    {
        out.travelTimeHours = timeSeconds / 3600.0;
    }
    else
    {
        const double avgSpeed =
            inputs.modeProperties
                  .value(PK::Mode::AverageSpeed, lk.defaultSpeed)
                  .toDouble();
        out.travelTimeHours = out.distanceKm / std::max(avgSpeed, 0.01);
    }

    out.fuelPerVehicle = inputs.modeProperties
                               .value(PK::Mode::AverageFuelConsumption, 0.0)
                               .toDouble();
    out.riskPerVehicle = inputs.modeProperties
                               .value(PK::Mode::RiskFactor, 0.01)
                               .toDouble();

    out.fuelType = inputs.modeProperties
                         .value(PK::Mode::FuelType).toString();
    if (out.fuelType.isEmpty())
        out.fuelType = QString::fromLatin1(lk.defaultFuel);

    const double calorific =
        inputs.fuelEnergy.value(out.fuelType, 10.0).toDouble();
    const double carbonPerUnit =
        inputs.fuelCarbonContent.value(out.fuelType, 2.68).toDouble();

    out.energyPerVehicle =
        out.fuelPerVehicle * out.distanceKm * calorific
        * inputs.locomotiveMultiplier;
    out.carbonPerVehicle =
        out.fuelPerVehicle * out.distanceKm * carbonPerUnit / 1000.0;

    out.valid = true;

    qCDebug(lcScenario) << "PathMetricsCalculator::compute:"
                        << "result -> distKm =" << out.distanceKm
                        << ", timeHrs =" << out.travelTimeHours
                        << ", energyPerVeh =" << out.energyPerVehicle
                        << ", carbonPerVeh =" << out.carbonPerVehicle;

    return out;
}

PathMetricsInputs gatherInputs(
    TransportationTypes::TransportationMode          mode,
    const ConfigController                          &config,
    const VehicleController                         *vehicles)
{
    qCDebug(lcScenario) << "PathMetricsCalculator::gatherInputs:"
                        << "mode =" << static_cast<int>(mode);

    using Mode = TransportationTypes::TransportationMode;
    PathMetricsInputs in;
    if (!modeSupported(mode)) return in;

    const ModeLookup lk = lookupFor(mode);
    in.modeProperties    = config.getTransportModes()
                                 .value(QString::fromLatin1(lk.configKey))
                                 .toMap();
    in.fuelEnergy        = config.getFuelEnergy();
    in.fuelCarbonContent = config.getFuelCarbonContent();

    if (in.modeProperties.isEmpty())
        qCWarning(lcScenario) << "PathMetricsCalculator::gatherInputs:"
                              << "no config properties found for mode key"
                              << lk.configKey;

    if (in.fuelEnergy.isEmpty())
        qCWarning(lcScenario) << "PathMetricsCalculator::gatherInputs:"
                              << "fuel energy config is empty";

    if (in.fuelCarbonContent.isEmpty())
        qCWarning(lcScenario) << "PathMetricsCalculator::gatherInputs:"
                              << "fuel carbon content config is empty";

    if (mode == Mode::Train && vehicles
        && !vehicles->getAllTrains().isEmpty())
    {
        // Deterministic: always use the first train (sorted by userId
        // via QMap). getRandomTrain() was non-deterministic — for
        // heterogeneous fleets, predicted and actual metrics would
        // reference different locomotive counts across calls.
        auto *t = vehicles->getAllTrains().first();
        if (t)
            in.locomotiveMultiplier = t->getLocomotives().count();
    }
    return in;
}

void projectPerContainer(PathMetrics &m, int containerCount, int capacity)
{
    qCDebug(lcScenario) << "PathMetricsCalculator::projectPerContainer:"
                        << "containerCount =" << containerCount
                        << ", capacity =" << capacity;

    if (!m.valid || containerCount <= 0) return;
    const int safeCapacity  = std::max(capacity, 1);
    const int vehiclesNeeded =
        (containerCount + safeCapacity - 1) / safeCapacity;

    m.containerCount     = containerCount;
    m.vehiclesNeeded     = std::max(vehiclesNeeded, 1);
    m.fuelPerContainer   = m.fuelPerVehicle   * m.vehiclesNeeded / containerCount;
    m.energyPerContainer = m.energyPerVehicle * m.vehiclesNeeded / containerCount;
    m.carbonPerContainer = m.carbonPerVehicle * m.vehiclesNeeded / containerCount;
    m.riskPerContainer   = m.riskPerVehicle   * m.vehiclesNeeded / containerCount;
}

PathMetrics compute(double                                   distanceMeters,
                    double                                   timeSeconds,
                    TransportationTypes::TransportationMode  mode,
                    const PathMetricsInputs                 &inputs,
                    int                                      containerCount)
{
    PathMetrics out = compute(distanceMeters, timeSeconds, mode, inputs);
    const int capacity =
        inputs.modeProperties
              .value(PK::Mode::AverageContainerNumber, 1)
              .toInt();
    qCDebug(lcScenario) << "PathMetricsCalculator::compute(5-arg):"
                        << "fuelPerVeh=" << out.fuelPerVehicle
                        << "energyPerVeh=" << out.energyPerVehicle
                        << "containers=" << containerCount
                        << "capacity=" << capacity;
    projectPerContainer(out, containerCount, capacity);
    return out;
}

} // namespace PathMetricsCalculator
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
