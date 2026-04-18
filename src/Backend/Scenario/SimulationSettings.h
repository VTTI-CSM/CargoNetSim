#pragma once

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
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
