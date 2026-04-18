#pragma once

#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Scenario-wide simulation parameters. Mirrors GUI/Widgets/SettingsWidget.h:140-175.
struct SimulationSettings
{
    int    timeStep              = 60;      ///< seconds
    int    shortestPathsN        = 5;
    double timeValueOfMoney      = 0.0;     ///< USD/hour
    bool   useSpecificTimeValues = false;
    double carbonRate            = 0.0;

    double shipMultiplier  = 1.0;
    double railMultiplier = 1.0;
    double truckMultiplier = 1.0;

    /// Per-mode parameters.
    struct Mode
    {
        double  speed       = 0.0;
        QString fuelType;
        double  fuelRate    = 0.0;
        int     containers  = 0;
        double  risk        = 0.0;
        double  timeValue   = 0.0;
        bool    useNetwork  = true;
    };
    Mode ship;
    Mode rail;
    Mode truck;

    /// Fuel metadata keyed by fuel-type name.
    struct Fuel
    {
        double energy = 0.0;
        double carbon = 0.0;
        double price  = 0.0;
    };
    QMap<QString, Fuel> fuelTypes;

    /// Global fallback dwell distribution.
    /// Method ∈ {"gamma", "exponential", "normal", "lognormal"}  — mirrors
    /// GUI/Widgets/PropertiesPanel.cpp:544 exactly.
    /// Parameters per method (serialized under YAML/JSON key "parameters" to
    /// match the existing per-terminal dwell_time schema at
    /// GUI/Items/TerminalItem.cpp:103 and PathFindingWorker.cpp:488):
    ///   gamma       → { "shape": double, "scale": double }
    ///   exponential → { "scale": double }
    ///   normal      → { "mean": double, "std_dev": double }
    ///   lognormal   → { "mean": double, "sigma": double }
    QString dwellMethod = QStringLiteral("normal");
    QMap<QString, QVariant> dwellParams =
        {{QStringLiteral("mean"),    2880.0},
         {QStringLiteral("std_dev"),  720.0}};   ///< serialized key: "parameters"

    double endTime = 86400.0;  ///< seconds
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
