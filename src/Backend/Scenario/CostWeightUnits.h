#pragma once

#include "PropertyKeys.h"

#include <QVariantMap>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace CostWeightUnits
{

inline double usdPerSecondFromUsdPerHour(double usdPerHour)
{
    return usdPerHour / 3600.0;
}

inline QVariantMap defaultCanonicalWeights(
    double timeValueUsdPerHour,
    double carbonTaxUsdPerTonne,
    double riskUsdPerFraction = 100.0)
{
    QVariantMap weights;
    weights[PropertyKeys::Segment::Cost] = 1.0;
    weights[PropertyKeys::Segment::TravelTime] =
        usdPerSecondFromUsdPerHour(timeValueUsdPerHour);
    weights[PropertyKeys::Segment::Distance] = 0.0;
    weights[PropertyKeys::Segment::CarbonEmissions] =
        carbonTaxUsdPerTonne;
    weights[PropertyKeys::Segment::Risk] = riskUsdPerFraction;
    weights[PropertyKeys::Segment::EnergyConsumption] = 1.0;
    weights[PropertyKeys::Segment::TerminalDelay] =
        usdPerSecondFromUsdPerHour(timeValueUsdPerHour);
    weights[PropertyKeys::Segment::TerminalCost] = 1.0;
    return weights;
}

inline void applyTimeValueUsdPerHour(
    QVariantMap &weights, double timeValueUsdPerHour)
{
    const double usdPerSecond =
        usdPerSecondFromUsdPerHour(timeValueUsdPerHour);
    weights[PropertyKeys::Segment::TravelTime] = usdPerSecond;
    weights[PropertyKeys::Segment::TerminalDelay] = usdPerSecond;
}

} // namespace CostWeightUnits
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
