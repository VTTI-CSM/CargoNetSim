#include "PathComparisonViewModel.h"

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"

namespace CargoNetSim
{
namespace GUI
{

namespace
{

const CargoNetSim::Backend::Scenario::PathExecutionResult *
executionResultFor(
    const PathComparisonViewModel::PathData *pathData)
{
    if (!pathData || !pathData->executionResult.has_value())
        return nullptr;
    return &pathData->executionResult.value();
}

CargoNetSim::Backend::PathSegment::SegmentMetricSnapshot
actualMetricsForSegment(
    const PathComparisonViewModel::PathData *pathData,
    int                                      segmentIndex)
{
    if (const auto *result = executionResultFor(pathData))
    {
        if (segmentIndex >= 0
            && segmentIndex < result->segmentResults.size())
        {
            return result->segmentResults[segmentIndex].actualMetrics;
        }
    }

    if (!pathData || !pathData->path)
        return {};

    const auto &segments = pathData->path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return segments[segmentIndex]->actualValues();
}

CargoNetSim::Backend::PathSegment::SegmentCostSnapshot
actualCostsForSegment(
    const PathComparisonViewModel::PathData *pathData,
    int                                      segmentIndex)
{
    if (const auto *result = executionResultFor(pathData))
    {
        if (segmentIndex >= 0
            && segmentIndex < result->segmentResults.size())
        {
            return result->segmentResults[segmentIndex].actualCosts;
        }
    }

    if (!pathData || !pathData->path)
        return {};

    const auto &segments = pathData->path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return segments[segmentIndex]->actualCosts();
}

const CargoNetSim::Backend::Scenario::TerminalExecutionResult *
terminalResultFor(
    const PathComparisonViewModel::PathData *pathData,
    int                                      terminalIndex)
{
    const auto *path = pathData ? pathData->path.get() : nullptr;
    const auto *result = executionResultFor(pathData);
    if (!path || !result)
        return nullptr;

    const auto &terminals = path->getTerminalsInPath();
    if (terminalIndex < 0 || terminalIndex >= terminals.size())
        return nullptr;

    const auto &terminal = terminals[terminalIndex];
    const int sequenceIndex =
        terminal.sequenceIndex >= 0
        ? terminal.sequenceIndex
        : terminalIndex;

    for (const auto &terminalResult : result->terminalResults)
    {
        if (terminalResult.terminalSequenceIndex != sequenceIndex)
            continue;
        if (terminalResult.scenarioTerminalId.isEmpty()
            || terminal.id.isEmpty()
            || terminalResult.scenarioTerminalId == terminal.id)
        {
            return &terminalResult;
        }
    }

    for (const auto &terminalResult : result->terminalResults)
    {
        if (terminal.id.isEmpty()
            || terminalResult.scenarioTerminalId == terminal.id)
        {
            return &terminalResult;
        }
    }

    return nullptr;
}

CargoNetSim::Backend::PathSegment::SegmentCostSnapshot
sumExecutionCosts(
    const QList<CargoNetSim::Backend::Scenario::SegmentExecutionResult>
        &segments)
{
    CargoNetSim::Backend::PathSegment::SegmentCostSnapshot total;
    for (const auto &segment : segments)
    {
        if (!segment.actualCosts.available)
            continue;

        total.available = true;
        total.travelTime += segment.actualCosts.travelTime;
        total.distance += segment.actualCosts.distance;
        total.carbonEmissions +=
            segment.actualCosts.carbonEmissions;
        total.energyConsumption +=
            segment.actualCosts.energyConsumption;
        total.risk += segment.actualCosts.risk;
        total.directCost += segment.actualCosts.directCost;
    }
    return total;
}

} // namespace

PathComparisonViewModel::PathComparisonViewModel(
    const QList<const PathData *> &paths)
    : m_paths(paths)
{
}

int PathComparisonViewModel::maxSegments() const
{
    int maxSegments = 0;
    for (const auto *pathData : m_paths)
    {
        if (!pathData || !pathData->path)
            continue;
        maxSegments = qMax(maxSegments,
                           pathData->path->getSegments().size());
    }
    return maxSegments;
}

int PathComparisonViewModel::maxTerminals() const
{
    int maxTerminals = 0;
    for (const auto *pathData : m_paths)
    {
        if (!pathData || !pathData->path)
            continue;
        maxTerminals = qMax(
            maxTerminals,
            pathData->path->getTerminalsInPath().size());
    }
    return maxTerminals;
}

QString PathComparisonViewModel::pathLabel(
    const PathData *pathData) const
{
    if (!pathData || !pathData->path)
        return QObject::tr("Unknown Path");
    return QObject::tr("Path %1").arg(pathData->path->getPathId());
}

QString PathComparisonViewModel::terminalDisplayName(
    const Backend::Path *path, const QString &terminalId) const
{
    if (!path)
        return QObject::tr("Unknown");

    for (const auto &terminal : path->getTerminalsInPath())
    {
        if (terminal.id == terminalId)
        {
            if (!terminal.displayName.isEmpty())
                return terminal.displayName;
            if (!terminal.canonicalName.isEmpty())
                return terminal.canonicalName;
            return terminal.id;
        }
    }

    return terminalId.isEmpty() ? QObject::tr("Unknown") : terminalId;
}

QString PathComparisonViewModel::terminalNameAt(
    const PathData *pathData, int terminalIndex) const
{
    if (!pathData || !pathData->path)
        return QObject::tr("N/A");

    const auto &terminals = pathData->path->getTerminalsInPath();
    if (terminalIndex < 0 || terminalIndex >= terminals.size())
        return QStringLiteral("-");

    const auto &terminal = terminals[terminalIndex];
    if (!terminal.displayName.isEmpty())
        return terminal.displayName;
    if (!terminal.canonicalName.isEmpty())
        return terminal.canonicalName;
    return terminal.id.isEmpty() ? QStringLiteral("-") : terminal.id;
}

PathComparisonViewModel::TerminalDisplayValues
PathComparisonViewModel::terminalValues(
    const PathData *pathData, int terminalIndex) const
{
    TerminalDisplayValues out;
    if (!pathData || !pathData->path)
        return out;

    const auto &terminals = pathData->path->getTerminalsInPath();
    if (terminalIndex < 0 || terminalIndex >= terminals.size())
        return out;

    const auto &terminal = terminals[terminalIndex];
    out.predictedAvailable = true;
    out.predictedHandlingSeconds = terminal.handlingTime;
    out.predictedDirectCostUsd = terminal.rawCost;
    out.predictedWeightedDelayContribution =
        terminal.weightedTerminalDelayContribution;
    out.predictedWeightedCostContribution =
        terminal.weightedTerminalCostContribution;
    out.predictedWeightedTotalContribution =
        terminal.weightedTerminalTotalContribution;
    out.predictedCostsSkipped = terminal.costsSkipped;
    out.predictedSkipReason = terminal.skipReason;

    const auto *actual = terminalResultFor(pathData, terminalIndex);
    if (!actual)
        return out;

    out.actualAvailable = true;
    out.actualArrivalMode = actual->arrivalMode;
    out.actualYardDwellSeconds = actual->actualYardDwellSeconds;
    out.actualCustomsDelaySeconds =
        actual->actualCustomsDelaySeconds;
    out.actualArrivalPenaltySeconds =
        actual->actualArrivalPenaltySeconds;
    out.actualTotalHandlingSeconds =
        actual->actualTotalHandlingSeconds;
    out.actualDirectCostUsd = actual->actualDirectCostUsd;
    out.actualWeightedDelayContribution =
        actual->actualWeightedDelayContribution;
    out.actualWeightedCostContribution =
        actual->actualWeightedCostContribution;
    out.actualWeightedTotalContribution =
        actual->actualWeightedTotalContribution;
    out.actualDroppedContainers =
        actual->totalDroppedContainers;
    out.actualArrivalEvents = actual->arrivalEvents;

    const auto state =
        actual->firstArrivalStateSnapshot
            .value(QStringLiteral("state")).toObject();
    out.actualUtilizationAtArrival =
        state.value(QStringLiteral("utilization")).toDouble(0.0);
    out.actualCongestionAtArrival =
        state.value(QStringLiteral("congestion")).toDouble(0.0);
    out.actualDelayMultiplierAtArrival =
        state.value(QStringLiteral("delay_multiplier")).toDouble(0.0);
    out.actualContainerCountAtArrival =
        actual->firstArrivalStateSnapshot
            .value(QStringLiteral("container_count")).toInt(0);
    return out;
}

QString PathComparisonViewModel::segmentDescription(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData || !pathData->path)
        return QObject::tr("N/A");

    const auto &segments = pathData->path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return QStringLiteral("-");
    }

    const auto *segment = segments[segmentIndex];
    return QStringLiteral("%1 → %2 (%3)")
        .arg(terminalDisplayName(pathData->path.get(), segment->getStart()),
             terminalDisplayName(pathData->path.get(), segment->getEnd()),
             Backend::TransportationTypes::toString(segment->getMode()));
}

PathComparisonViewModel::SegmentDisplayValues
PathComparisonViewModel::segmentValues(
    const PathData *pathData, int segmentIndex) const
{
    SegmentDisplayValues out;
    if (!pathData || !pathData->path)
        return out;

    const auto &segments = pathData->path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return out;
    }

    const auto *segment = segments[segmentIndex];
    const auto predicted = segment->estimatedValues();
    out.predictedAvailable = predicted.available;
    out.predictedDistanceKm =
        Backend::Units::LengthMeters(predicted.distance)
            .convert<units::length::kilometer>()
            .value();
    out.predictedTravelTimeHours =
        Backend::Units::TimeSeconds(predicted.travelTime)
            .convert<units::time::hour>()
            .value();
    out.predictedCarbonEmissions = predicted.carbonEmissions;
    out.predictedEnergyConsumption = predicted.energyConsumption;
    out.predictedRisk = predicted.risk;

    const auto actual = actualMetricsForSegment(pathData, segmentIndex);
    out.actualAvailable = actual.available;
    out.actualDistanceKm =
        Backend::Units::LengthMeters(actual.distance)
            .convert<units::length::kilometer>()
            .value();
    out.actualTravelTimeHours =
        Backend::Units::TimeSeconds(actual.travelTime)
            .convert<units::time::hour>()
            .value();
    out.actualCarbonEmissions = actual.carbonEmissions;
    out.actualEnergyConsumption = actual.energyConsumption;
    out.actualRisk = actual.risk;
    return out;
}

Backend::PathSegment::SegmentCostSnapshot
PathComparisonViewModel::segmentPredictedCosts(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData || !pathData->path)
        return {};

    const auto &segments = pathData->path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return segments[segmentIndex]->estimatedCosts();
}

Backend::PathSegment::SegmentCostSnapshot
PathComparisonViewModel::segmentActualCosts(
    const PathData *pathData, int segmentIndex) const
{
    return actualCostsForSegment(pathData, segmentIndex);
}

PathComparisonViewModel::PathCostTotals
PathComparisonViewModel::pathCostTotals(
    const PathData *pathData) const
{
    PathCostTotals out;
    if (!pathData || !pathData->path)
        return out;

    const auto segments = pathData->path->getSegments();
    out.predicted = sumCostSnapshots(segments, false);
    if (const auto *result = executionResultFor(pathData))
        out.actual = sumExecutionCosts(result->segmentResults);
    else
        out.actual = sumCostSnapshots(segments, true);
    return out;
}

QString PathComparisonViewModel::simulatedCostText(
    const PathData *pathData) const
{
    if (!pathData || pathData->m_totalSimulationPathCost < 0.0)
        return QObject::tr("Not simulated");
    return QString::number(pathData->m_totalSimulationPathCost,
                           'f', 2);
}

QString PathComparisonViewModel::simulatedEdgeCostText(
    const PathData *pathData) const
{
    if (!pathData || pathData->m_totalSimulationEdgeCosts < 0.0)
        return QObject::tr("Not simulated");
    return QString::number(pathData->m_totalSimulationEdgeCosts,
                           'f', 2);
}

QString PathComparisonViewModel::simulatedTerminalCostText(
    const PathData *pathData) const
{
    if (!pathData
        || pathData->m_totalSimulationTerminalCosts < 0.0)
    {
        return QObject::tr("Not simulated");
    }
    return QString::number(
        pathData->m_totalSimulationTerminalCosts, 'f', 2);
}

QString PathComparisonViewModel::costDifferenceText(
    const PathData *pathData) const
{
    if (!pathData || !pathData->path
        || pathData->m_totalSimulationPathCost < 0.0
        || pathData->path->getTotalPathCost() <= 0.0)
    {
        return QObject::tr("N/A");
    }

    const double predicted = pathData->path->getTotalPathCost();
    const double actual = pathData->m_totalSimulationPathCost;
    const double difference =
        ((actual - predicted) / predicted) * 100.0;
    return difference > 0.0
        ? QStringLiteral("+%1%")
              .arg(QString::number(difference, 'f', 2))
        : QStringLiteral("%1%")
              .arg(QString::number(difference, 'f', 2));
}

Backend::PathSegment::SegmentCostSnapshot
PathComparisonViewModel::sumCostSnapshots(
    const QList<Backend::PathSegment *> &segments,
    bool                                 useActualCosts)
{
    Backend::PathSegment::SegmentCostSnapshot total;
    for (const auto *segment : segments)
    {
        if (!segment)
            continue;

        const auto snapshot =
            useActualCosts ? segment->actualCosts()
                           : segment->estimatedCosts();
        if (!snapshot.available)
            continue;

        total.available = true;
        total.travelTime += snapshot.travelTime;
        total.distance += snapshot.distance;
        total.carbonEmissions += snapshot.carbonEmissions;
        total.energyConsumption += snapshot.energyConsumption;
        total.risk += snapshot.risk;
        total.directCost += snapshot.directCost;
    }
    return total;
}

} // namespace GUI
} // namespace CargoNetSim
