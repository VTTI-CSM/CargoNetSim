#pragma once

#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Compile-safe named constants for property-bag string keys used
/// across the codebase. A misspelled constant name fails at compile
/// time; a misspelled string literal fails silently at runtime.
///
/// Three groups match the three QVariantMap schemas:
///   Mode     — keys inside per-mode maps from ConfigController
///   Terminal — keys inside TerminalPlacement::properties
///   Segment  — keys inside PathSegment::attributes sub-objects
namespace PropertyKeys {

namespace Mode {
    inline const QString AverageSpeed            = QStringLiteral("average_speed");
    inline const QString AverageFuelConsumption  = QStringLiteral("average_fuel_consumption");
    inline const QString AverageContainerNumber  = QStringLiteral("average_container_number");
    inline const QString RiskFactor              = QStringLiteral("risk_factor");
    inline const QString FuelType                = QStringLiteral("fuel_type");
    inline const QString UseNetwork              = QStringLiteral("use_network");
    inline const QString TimeValueOfMoney        = QStringLiteral("time_value_of_money");
} // namespace Mode

namespace Terminal {
    inline const QString InitialContainerCount   = QStringLiteral("initial_container_count");
    inline const QString DestinationTerminal     = QStringLiteral("destination_terminal");
    inline const QString Destinations            = QStringLiteral("destinations");
    inline const QString DestTerminal            = QStringLiteral("terminal");
    inline const QString DestFraction            = QStringLiteral("fraction");
} // namespace Terminal

namespace Segment {
    // Sub-object container names
    inline const QString Estimated               = QStringLiteral("estimated");
    /// Preserved as "actual_values" (not "actual") for backward compat
    /// with raw-read sites in PathComparisonDialog / PathReportGenerator.
    inline const QString ActualValues            = QStringLiteral("actual_values");
    inline const QString ActualCost              = QStringLiteral("actual_cost");

    // Metric keys inside the sub-objects
    inline const QString TravelTime              = QStringLiteral("travelTime");
    inline const QString Distance                = QStringLiteral("distance");
    inline const QString CarbonEmissions         = QStringLiteral("carbonEmissions");
    inline const QString EnergyConsumption       = QStringLiteral("energyConsumption");
    inline const QString Risk                    = QStringLiteral("risk");
    inline const QString Cost                    = QStringLiteral("cost");
} // namespace Segment

} // namespace PropertyKeys
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
