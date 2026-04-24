#include "PathComparisonViewModel.h"

#include "Backend/Commons/TransportationMode.h"

namespace CargoNetSim
{
namespace GUI
{

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
    out.predictedDistanceKm = predicted.distance / 1000.0;
    out.predictedTravelTimeHours = predicted.travelTime / 3600.0;
    out.predictedCarbonEmissions = predicted.carbonEmissions;
    out.predictedEnergyConsumption = predicted.energyConsumption;
    out.predictedRisk = predicted.risk;

    const auto actual = segment->actualValues();
    out.actualAvailable = actual.available;
    out.actualDistanceKm = actual.distance / 1000.0;
    out.actualTravelTimeHours = actual.travelTime / 3600.0;
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

PathComparisonViewModel::PathCostTotals
PathComparisonViewModel::pathCostTotals(
    const PathData *pathData) const
{
    PathCostTotals out;
    if (!pathData || !pathData->path)
        return out;

    const auto segments = pathData->path->getSegments();
    out.predicted = sumCostSnapshots(segments, false);
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
