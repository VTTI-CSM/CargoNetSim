#pragma once

#include "Backend/Application/PathPresentationService.h"

namespace CargoNetSim
{
namespace GUI
{

class PathComparisonViewModel
{
public:
    using PathData = Backend::Application::PathPresentationRecord;
    using SegmentDisplayValues =
        Backend::Application::PathPresentationSegmentDisplayValues;
    using PathCostTotals =
        Backend::Application::PathPresentationCostTotals;
    using TerminalDisplayValues =
        Backend::Application::PathPresentationTerminalDisplayValues;
    using CostSnapshot =
        Backend::Application::PathPresentationCostSnapshot;
    using PathSummary =
        Backend::Application::PathPresentationSummary;
    using TerminalEntry =
        Backend::Application::PathPresentationTerminalEntry;
    using SegmentEntry =
        Backend::Application::PathPresentationSegmentEntry;

    explicit PathComparisonViewModel(
        const QList<const PathData *> &paths);

    int maxSegments() const;
    int maxTerminals() const;

    PathSummary pathSummary(const PathData *pathData) const;
    QList<TerminalEntry> terminalEntries(
        const PathData *pathData) const;
    QList<SegmentEntry> segmentEntries(
        const PathData *pathData) const;
    QString pathLabel(const PathData *pathData) const;
    QString terminalDisplayName(const PathData *pathData,
                                const QString  &terminalId) const;
    QString terminalNameAt(const PathData *pathData,
                           int             terminalIndex) const;
    TerminalDisplayValues terminalValues(
        const PathData *pathData,
        int             terminalIndex) const;
    QString segmentDescription(const PathData *pathData,
                               int             segmentIndex) const;

    SegmentDisplayValues segmentValues(
        const PathData *pathData,
        int             segmentIndex) const;
    CostSnapshot segmentPredictedCosts(
        const PathData *pathData,
        int             segmentIndex) const;
    CostSnapshot segmentActualCosts(
        const PathData *pathData,
        int             segmentIndex) const;
    PathCostTotals pathCostTotals(
        const PathData *pathData) const;

    QString simulatedCostText(const PathData *pathData) const;
    QString simulatedEdgeCostText(const PathData *pathData) const;
    QString simulatedTerminalCostText(const PathData *pathData) const;
    QString costDifferenceText(const PathData *pathData) const;

private:
    const QList<const PathData *> &m_paths;
    Backend::Application::PathPresentationService m_service;
};

} // namespace GUI
} // namespace CargoNetSim
