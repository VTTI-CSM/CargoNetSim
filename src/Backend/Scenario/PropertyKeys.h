#pragma once

#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Compile-safe named constants for property-bag string keys used
/// across the codebase. A misspelled constant name fails at compile
/// time; a misspelled string literal fails silently at runtime.
///
/// Four groups match the four QVariantMap schemas:
///   Mode       — keys inside per-mode maps from ConfigController
///   Terminal   — keys inside TerminalPlacement::properties
///   Segment    — keys inside PathSegment::attributes sub-objects
///   Simulation — keys inside the simulation-section scalar map
namespace PropertyKeys {

namespace Mode {
    inline const QString AverageSpeed            = QStringLiteral("average_speed");
    inline const QString AverageFuelConsumption  = QStringLiteral("average_fuel_consumption");
    inline const QString AverageContainerNumber  = QStringLiteral("average_container_number");
    inline const QString AverageLocomotiveCount  = QStringLiteral("average_locomotive_count");
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
    inline const QString EstimatedCost           = QStringLiteral("estimated_cost");

    // Metric keys inside the sub-objects
    inline const QString TravelTime              = QStringLiteral("travelTime");
    inline const QString Distance                = QStringLiteral("distance");
    inline const QString CarbonEmissions         = QStringLiteral("carbonEmissions");
    inline const QString EnergyConsumption       = QStringLiteral("energyConsumption");
    inline const QString Risk                    = QStringLiteral("risk");
    inline const QString Cost                    = QStringLiteral("cost");

    // Cost-function weight keys (keys in the per-mode weight maps from
    // getCostFunctionWeights() and TerminalSim cost-function contracts.
    inline const QString TerminalDelay           = QStringLiteral("terminal_delay");
    inline const QString TerminalCost            = QStringLiteral("terminal_cost");
} // namespace Segment

/// Keys for the simulation-section scalar map (the flat QVariantMap stored
/// under m_config["simulation"]). These keys are written by SettingsWidget
/// and read by ConfigController::getTimeValueOfMoney() / getCostFunctionWeights().
///
/// Note: Simulation::TimeValueOfMoney shares the string "time_value_of_money"
/// with Mode::TimeValueOfMoney. They are semantically distinct: Mode refers to
/// the per-mode entry inside transport_modes[mode], Simulation refers to the
/// global average scalar inside the simulation section.
namespace Simulation {
    inline const QString TimeValueOfMoney  = QStringLiteral("time_value_of_money");
    inline const QString UseModeSpecific   = QStringLiteral("use_mode_specific");
} // namespace Simulation

} // namespace PropertyKeys
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
