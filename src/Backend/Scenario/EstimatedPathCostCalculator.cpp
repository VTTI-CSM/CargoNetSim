#include "EstimatedPathCostCalculator.h"

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "PropertyKeys.h"

#include <QtGlobal>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace
{

namespace PK = PropertyKeys;
using Mode = TransportationTypes::TransportationMode;

QVariantMap weightsForMode(const QVariantMap &costFunctionWeights,
                            Mode mode)
{
    const QString modeKey =
        QString::number(static_cast<int>(mode));
    return costFunctionWeights.contains(modeKey)
        ? costFunctionWeights.value(modeKey).toMap()
        : costFunctionWeights.value(QStringLiteral("default")).toMap();
}

int capacityForMode(const QVariantMap &transportModes, Mode mode)
{
    const QString key = transportationModeToString(mode);
    return qMax(1, transportModes.value(key)
                    .toMap()
                    .value(PK::Mode::AverageContainerNumber, 1)
                    .toInt());
}

int vehiclesNeeded(int containerCount, int capacity)
{
    if (containerCount <= 0)
        return 0;
    const int safeCapacity = qMax(1, capacity);
    return qMax(1, (containerCount + safeCapacity - 1) / safeCapacity);
}

double loadShare(int containerCount, int vehicleCount, int capacity)
{
    if (containerCount <= 0 || vehicleCount <= 0 || capacity <= 0)
        return 0.0;
    return static_cast<double>(containerCount)
           / static_cast<double>(vehicleCount * capacity);
}

void addCostToBreakdown(QJsonObject &breakdown,
                        const QString &key,
                        double value)
{
    breakdown[key] = breakdown.value(key).toDouble(0.0) + value;
}

} // namespace

namespace EstimatedPathCostCalculator
{

PathSegment::SegmentCostSnapshot computeSegmentCost(
    const PathSegment &segment,
    const QVariantMap &costFunctionWeights,
    const QVariantMap &transportModes,
    int                containerCount)
{
    PathSegment::SegmentCostSnapshot out;

    const auto metrics = segment.estimatedValues();
    if (!metrics.available)
        return out;

    const Mode mode = segment.getMode();
    const QVariantMap weights =
        weightsForMode(costFunctionWeights, mode);
    const int capacity = capacityForMode(transportModes, mode);
    const int vehicleCount = vehiclesNeeded(containerCount, capacity);
    const double share = loadShare(containerCount, vehicleCount,
                                   capacity);

    const auto terminalSimCosts = segment.estimatedCosts();

    out.available = true;
    out.travelTime =
        metrics.travelTime
        * weights.value(PK::Segment::TravelTime).toDouble();
    out.distance =
        metrics.distance
        * weights.value(PK::Segment::Distance).toDouble();
    out.carbonEmissions =
        metrics.carbonEmissions * share
        * weights.value(PK::Segment::CarbonEmissions).toDouble();
    out.energyConsumption =
        metrics.energyConsumption * share
        * weights.value(PK::Segment::EnergyConsumption).toDouble();
    out.risk =
        metrics.risk
        * weights.value(PK::Segment::Risk).toDouble();
    out.directCost =
        terminalSimCosts.available
        ? terminalSimCosts.directCost
        : 0.0;
    return out;
}

EstimatedPathCost compute(const Path &path,
                          const QVariantMap &costFunctionWeights,
                          const QVariantMap &transportModes,
                          int                containerCount)
{
    EstimatedPathCost out;

    QJsonObject weightedEdge;
    weightedEdge[PK::Segment::CarbonEmissions] = 0.0;
    weightedEdge[PK::Segment::Cost] = 0.0;
    weightedEdge[PK::Segment::Distance] = 0.0;
    weightedEdge[PK::Segment::EnergyConsumption] = 0.0;
    weightedEdge[PK::Segment::Risk] = 0.0;
    weightedEdge[PK::Segment::TravelTime] = 0.0;

    QJsonObject weightedTerminal;
    weightedTerminal[QStringLiteral("delay")] = 0.0;
    weightedTerminal[QStringLiteral("direct_cost")] = 0.0;

    auto segments = path.getSegments();
    for (int i = 0; i < segments.size(); ++i)
    {
        const auto *segment = segments.at(i);
        if (!segment)
            continue;

        const auto metrics = segment->estimatedValues();
        if (metrics.available)
        {
            out.metrics.valid = true;
            out.metrics.distanceKm +=
                Units::toKilometers(
                    Units::meters(metrics.distance)).value();
            out.metrics.travelTimeHours +=
                Units::toHours(
                    Units::seconds(metrics.travelTime)).value();
            out.metrics.energyPerVehicle +=
                metrics.energyConsumption;
            out.metrics.carbonPerVehicle +=
                metrics.carbonEmissions;
            out.metrics.riskPerVehicle += metrics.risk;

            const int capacity =
                capacityForMode(transportModes, segment->getMode());
            const int vehicleCount =
                vehiclesNeeded(containerCount, capacity);
            const double share =
                loadShare(containerCount, vehicleCount, capacity);
            out.metrics.energyPerContainer +=
                metrics.energyConsumption * share;
            out.metrics.carbonPerContainer +=
                metrics.carbonEmissions * share;
            out.metrics.riskPerContainer += metrics.risk;

            PathMetrics::VehicleRequirement requirement;
            requirement.segmentIndex =
                segment->sequenceIndex() >= 0
                ? segment->sequenceIndex()
                : i;
            requirement.mode = segment->getMode();
            requirement.vehiclesNeeded = vehicleCount;
            out.metrics.previewVehicleBreakdown.append(
                requirement);
            out.metrics.vehiclesNeeded += vehicleCount;
        }

        const auto costs = computeSegmentCost(
            *segment, costFunctionWeights, transportModes,
            containerCount);
        if (!costs.available)
            continue;

        out.edgeCost += costs.total();
        addCostToBreakdown(weightedEdge, PK::Segment::CarbonEmissions,
                           costs.carbonEmissions);
        addCostToBreakdown(weightedEdge, PK::Segment::Cost,
                           costs.directCost);
        addCostToBreakdown(weightedEdge, PK::Segment::Distance,
                           costs.distance);
        addCostToBreakdown(weightedEdge, PK::Segment::EnergyConsumption,
                           costs.energyConsumption);
        addCostToBreakdown(weightedEdge, PK::Segment::Risk,
                           costs.risk);
        addCostToBreakdown(weightedEdge, PK::Segment::TravelTime,
                           costs.travelTime);
    }

    out.metrics.containerCount = qMax(0, containerCount);
    if (out.metrics.containerCount > 0)
    {
        out.metrics.energyPerContainer /=
            out.metrics.containerCount;
        out.metrics.carbonPerContainer /=
            out.metrics.containerCount;
        out.metrics.riskPerContainer /=
            out.metrics.containerCount;
    }

    // Terminal costs belong to TerminalSim. CargoNetSim only carries the
    // predicted terminal total returned by TerminalSim path discovery and
    // later replaces it with TerminalSim execution records after a run.
    out.terminalCost = path.getTotalTerminalCosts();
    out.totalCost = out.edgeCost + out.terminalCost;

    out.costBreakdown = path.getCostBreakdown();
    out.costBreakdown[QStringLiteral("weighted_edge")] =
        weightedEdge;
    if (!out.costBreakdown.contains(QStringLiteral("weighted_terminal")))
    {
        weightedTerminal[QStringLiteral("delay")] =
            path.getWeightedTerminalDelayTotal();
        weightedTerminal[QStringLiteral("direct_cost")] =
            path.getWeightedTerminalDirectCostTotal();
        out.costBreakdown[QStringLiteral("weighted_terminal")] =
            weightedTerminal;
    }

    return out;
}

} // namespace EstimatedPathCostCalculator
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
