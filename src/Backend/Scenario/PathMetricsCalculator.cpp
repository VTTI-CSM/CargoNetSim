// src/Backend/Scenario/PathMetricsCalculator.cpp
#include "PathMetricsCalculator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/ConfigController.h"
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

double locomotiveCountForMode(TransportationTypes::TransportationMode mode,
                              const QVariantMap &modeProperties)
{
    using Mode = TransportationTypes::TransportationMode;
    if (mode != Mode::Train)
        return 1.0;

    return std::max(
        1.0,
        modeProperties.value(PK::Mode::AverageLocomotiveCount, 1.0)
            .toDouble());
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

    out.setDistance(Units::toKilometers(
        Units::meters(distanceMeters)));

    if (useNetwork)
    {
        out.setTravelTime(Units::toHours(
            Units::seconds(timeSeconds)));
    }
    else
    {
        const double avgSpeed =
            inputs.modeProperties
                  .value(PK::Mode::AverageSpeed, lk.defaultSpeed)
                  .toDouble();
        out.setTravelTime(Units::hours(
            out.distanceKm / std::max(avgSpeed, 0.01)));
    }

    const double fuelRatePerLocomotive =
        inputs.modeProperties
            .value(PK::Mode::AverageFuelConsumption, 0.0)
            .toDouble();
    const double locomotiveCount =
        locomotiveCountForMode(mode, inputs.modeProperties);
    out.fuelPerVehicle =
        fuelRatePerLocomotive * out.distanceKm * locomotiveCount;
    out.setRiskPerVehicle(Units::scalar(
        inputs.modeProperties
            .value(PK::Mode::RiskFactor, 0.01)
            .toDouble()));

    out.fuelType = inputs.modeProperties
                         .value(PK::Mode::FuelType).toString();
    if (out.fuelType.isEmpty())
        out.fuelType = QString::fromLatin1(lk.defaultFuel);

    const double calorific =
        inputs.fuelEnergy.value(out.fuelType, 10.0).toDouble();
    const double carbonPerUnit =
        inputs.fuelCarbonContent.value(out.fuelType, 2.68).toDouble();

    out.setEnergyPerVehicle(Units::kilowattHours(
        out.fuelPerVehicle * calorific));
    out.setCarbonPerVehicle(
        Units::toMetricTons(Units::kilograms(
            out.fuelPerVehicle * carbonPerUnit)));

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
    const ConfigController                          &config)
{
    qCDebug(lcScenario) << "PathMetricsCalculator::gatherInputs:"
                        << "mode =" << static_cast<int>(mode);

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

    return in;
}

PathMetricsInputs gatherInputs(
    TransportationTypes::TransportationMode mode,
    const ConfigController                 &config,
    const SimulationSettings               &settings)
{
    PathMetricsInputs in = gatherInputs(mode, config);
    if (in.modeProperties.isEmpty())
        return in;

    const SimulationSettings::Mode *modeSettings = nullptr;
    using Mode = TransportationTypes::TransportationMode;
    switch (mode)
    {
    case Mode::Ship:
        modeSettings = &settings.ship;
        break;
    case Mode::Train:
        modeSettings = &settings.rail;
        break;
    case Mode::Truck:
        modeSettings = &settings.truck;
        break;
    default:
        return in;
    }

    if (const auto speed = modeSettings->speedUnits())
        in.modeProperties[PK::Mode::AverageSpeed] = speed->value();
    if (modeSettings->fuelType.has_value())
        in.modeProperties[PK::Mode::FuelType] =
            modeSettings->fuelType.value();
    if (modeSettings->fuelRate.has_value())
        in.modeProperties[PK::Mode::AverageFuelConsumption] =
            modeSettings->fuelRate.value();
    if (modeSettings->containers.has_value())
        in.modeProperties[PK::Mode::AverageContainerNumber] =
            modeSettings->containers.value();
    if (modeSettings->locomotives.has_value())
        in.modeProperties[PK::Mode::AverageLocomotiveCount] =
            modeSettings->locomotives.value();
    if (const auto risk = modeSettings->riskUnits())
        in.modeProperties[PK::Mode::RiskFactor] = risk->value();
    if (modeSettings->timeValue.has_value())
        in.modeProperties[PK::Mode::TimeValueOfMoney] =
            modeSettings->timeValue.value();
    if (modeSettings->useNetwork.has_value())
        in.modeProperties[PK::Mode::UseNetwork] =
            modeSettings->useNetwork.value();

    for (auto it = settings.fuelTypes.constBegin();
         it != settings.fuelTypes.constEnd(); ++it)
    {
        const QString &fuelName = it.key();
        const auto &fuel = it.value();
        if (fuel.energy.has_value())
            in.fuelEnergy[fuelName] = fuel.energy.value();
        if (fuel.carbon.has_value())
            in.fuelCarbonContent[fuelName] = fuel.carbon.value();
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
