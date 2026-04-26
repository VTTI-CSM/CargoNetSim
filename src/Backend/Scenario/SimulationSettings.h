#pragma once

#include "Backend/Commons/Units.h"

#include <cmath>
#include <optional>
#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Scenario-wide simulation parameters. All fields are optional — a nullopt
/// field means "not specified in YAML; inherit the config.xml default at apply
/// time." See ScenarioApplier::applySettings() for the merge logic.
struct SimulationSettings
{
    std::optional<int>    timeStep;             ///< seconds
    std::optional<int>    shortestPathsN;
    std::optional<double> timeValueOfMoney;     ///< USD/hour
    std::optional<bool>   useSpecificTimeValues;
    std::optional<double> carbonRate;

    std::optional<double> shipMultiplier;
    std::optional<double> railMultiplier;
    std::optional<double> truckMultiplier;

    /// Per-mode parameters.
    struct Mode
    {
        std::optional<double>  speed;
        std::optional<QString> fuelType;
        std::optional<double>  fuelRate;
        std::optional<int>     containers;
        std::optional<double>  risk;
        std::optional<double>  timeValue;
        std::optional<bool>    useNetwork;

        std::optional<Units::SpeedKilometersPerHour> speedUnits() const
        {
            if (!speed.has_value())
                return std::nullopt;
            return Units::kilometersPerHour(speed.value());
        }

        void setSpeed(Units::SpeedKilometersPerHour value)
        {
            speed = value.value();
        }

        std::optional<Units::Scalar> riskUnits() const
        {
            if (!risk.has_value())
                return std::nullopt;
            return Units::scalar(risk.value());
        }

        void setRisk(Units::Scalar value)
        {
            risk = value.value();
        }
    };
    Mode ship;
    Mode rail;
    Mode truck;

    /// Fuel metadata keyed by fuel-type name. Map presence = fuel was in YAML.
    struct Fuel
    {
        std::optional<double> energy;
        std::optional<double> carbon;
        std::optional<double> price;
    };
    QMap<QString, Fuel> fuelTypes;

    /// Global fallback dwell distribution.
    /// Method ∈ {"gamma", "exponential", "normal", "lognormal"}
    /// Parameters per method (serialized under YAML/JSON key "parameters"):
    ///   gamma       → { "shape": double, "scale": double }
    ///   exponential → { "scale": double }
    ///   normal      → { "mean": double, "std_dev": double }
    ///   lognormal   → { "mean": double, "sigma": double }
    std::optional<QString>               dwellMethod;
    std::optional<QMap<QString, QVariant>> dwellParams;

    std::optional<double> endTime;  ///< seconds

    std::optional<Units::TimeSeconds> timeStepUnits() const
    {
        if (!timeStep.has_value())
            return std::nullopt;
        return Units::seconds(static_cast<double>(timeStep.value()));
    }

    void setTimeStep(Units::TimeSeconds value)
    {
        timeStep = static_cast<int>(std::lround(value.value()));
    }

    std::optional<Units::TimeSeconds> endTimeUnits() const
    {
        if (!endTime.has_value())
            return std::nullopt;
        return Units::seconds(endTime.value());
    }

    void setEndTime(Units::TimeSeconds value)
    {
        endTime = value.value();
    }
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
