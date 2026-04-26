#include "PathComparisonViewModel.h"

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
    return m_service.maxSegments(m_paths);
}

int PathComparisonViewModel::maxTerminals() const
{
    return m_service.maxTerminals(m_paths);
}

PathComparisonViewModel::PathSummary
PathComparisonViewModel::pathSummary(
    const PathData *pathData) const
{
    if (!pathData)
    {
        PathSummary out;
        out.pathLabel = QObject::tr("Unknown Path");
        out.startTerminalName = QObject::tr("Unknown");
        out.endTerminalName = QObject::tr("Unknown");
        return out;
    }
    return m_service.summary(*pathData);
}

QList<PathComparisonViewModel::TerminalEntry>
PathComparisonViewModel::terminalEntries(
    const PathData *pathData) const
{
    if (!pathData)
        return {};
    return m_service.terminalEntries(*pathData);
}

QList<PathComparisonViewModel::SegmentEntry>
PathComparisonViewModel::segmentEntries(
    const PathData *pathData) const
{
    if (!pathData)
        return {};
    return m_service.segmentEntries(*pathData);
}

QString PathComparisonViewModel::pathLabel(
    const PathData *pathData) const
{
    return pathSummary(pathData).pathLabel;
}

QString PathComparisonViewModel::terminalDisplayName(
    const PathData *pathData, const QString &terminalId) const
{
    if (!pathData)
        return QObject::tr("Unknown");
    return m_service.terminalDisplayName(*pathData, terminalId);
}

QString PathComparisonViewModel::terminalNameAt(
    const PathData *pathData, int terminalIndex) const
{
    if (!pathData)
        return QObject::tr("N/A");
    return m_service.terminalNameAt(*pathData, terminalIndex);
}

PathComparisonViewModel::TerminalDisplayValues
PathComparisonViewModel::terminalValues(
    const PathData *pathData, int terminalIndex) const
{
    if (!pathData)
        return {};
    return m_service.terminalValues(*pathData, terminalIndex);
}

QString PathComparisonViewModel::segmentDescription(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData)
        return QObject::tr("N/A");
    return m_service.segmentDescription(*pathData, segmentIndex);
}

PathComparisonViewModel::SegmentDisplayValues
PathComparisonViewModel::segmentValues(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData)
        return {};
    return m_service.segmentValues(*pathData, segmentIndex);
}

PathComparisonViewModel::CostSnapshot
PathComparisonViewModel::segmentPredictedCosts(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData)
        return {};
    return m_service.segmentPredictedCosts(*pathData,
                                           segmentIndex);
}

PathComparisonViewModel::CostSnapshot
PathComparisonViewModel::segmentActualCosts(
    const PathData *pathData, int segmentIndex) const
{
    if (!pathData)
        return {};
    return m_service.segmentActualCosts(*pathData,
                                        segmentIndex);
}

PathComparisonViewModel::PathCostTotals
PathComparisonViewModel::pathCostTotals(
    const PathData *pathData) const
{
    if (!pathData)
        return {};
    return m_service.pathCostTotals(*pathData);
}

QString PathComparisonViewModel::simulatedCostText(
    const PathData *pathData) const
{
    if (!pathData || !pathData->hasSimulationTotalCost())
        return QObject::tr("Not simulated");
    return QString::number(pathData->simulationTotalCost,
                           'f', 2);
}

QString PathComparisonViewModel::simulatedEdgeCostText(
    const PathData *pathData) const
{
    if (!pathData || !pathData->hasSimulationEdgeCosts())
        return QObject::tr("Not simulated");
    return QString::number(pathData->simulationEdgeCosts,
                           'f', 2);
}

QString PathComparisonViewModel::simulatedTerminalCostText(
    const PathData *pathData) const
{
    if (!pathData
        || !pathData->hasSimulationTerminalCosts())
    {
        return QObject::tr("Not simulated");
    }
    return QString::number(
        pathData->simulationTerminalCosts, 'f', 2);
}

QString PathComparisonViewModel::costDifferenceText(
    const PathData *pathData) const
{
    if (!pathData || !pathData->path
        || !pathData->hasSimulationTotalCost()
        || pathData->path->getTotalPathCost() <= 0.0)
    {
        return QObject::tr("N/A");
    }

    const double predicted = pathData->path->getTotalPathCost();
    const double actual = pathData->simulationTotalCost;
    const double difference =
        ((actual - predicted) / predicted) * 100.0;
    return difference > 0.0
        ? QStringLiteral("+%1%")
              .arg(QString::number(difference, 'f', 2))
        : QStringLiteral("%1%")
              .arg(QString::number(difference, 'f', 2));
}

} // namespace GUI
} // namespace CargoNetSim
