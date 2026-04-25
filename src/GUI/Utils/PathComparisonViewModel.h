#pragma once

#include "GUI/Widgets/ShortestPathTable.h"

namespace CargoNetSim
{
namespace GUI
{

class PathComparisonViewModel
{
public:
    using PathData = ShortestPathsTable::PathData;

    struct SegmentDisplayValues
    {
        bool   predictedAvailable = false;
        double predictedDistanceKm = 0.0;
        double predictedTravelTimeHours = 0.0;
        double predictedCarbonEmissions = 0.0;
        double predictedEnergyConsumption = 0.0;
        double predictedRisk = 0.0;

        bool   actualAvailable = false;
        double actualDistanceKm = 0.0;
        double actualTravelTimeHours = 0.0;
        double actualCarbonEmissions = 0.0;
        double actualEnergyConsumption = 0.0;
        double actualRisk = 0.0;
    };

    struct PathCostTotals
    {
        Backend::PathSegment::SegmentCostSnapshot predicted;
        Backend::PathSegment::SegmentCostSnapshot actual;
    };

    struct TerminalDisplayValues
    {
        bool   predictedAvailable = false;
        double predictedHandlingSeconds = 0.0;
        double predictedDirectCostUsd = 0.0;
        double predictedWeightedDelayContribution = 0.0;
        double predictedWeightedCostContribution = 0.0;
        double predictedWeightedTotalContribution = 0.0;
        bool   predictedCostsSkipped = false;
        QString predictedSkipReason;

        bool   actualAvailable = false;
        Backend::TransportationTypes::TransportationMode actualArrivalMode =
            Backend::TransportationTypes::TransportationMode::Any;
        double actualYardDwellSeconds = 0.0;
        double actualCustomsDelaySeconds = 0.0;
        double actualArrivalPenaltySeconds = 0.0;
        double actualTotalHandlingSeconds = 0.0;
        double actualDirectCostUsd = 0.0;
        double actualWeightedDelayContribution = 0.0;
        double actualWeightedCostContribution = 0.0;
        double actualWeightedTotalContribution = 0.0;
        int    actualDroppedContainers = 0;
        int    actualArrivalEvents = 0;
        double actualUtilizationAtArrival = 0.0;
        double actualCongestionAtArrival = 0.0;
        double actualDelayMultiplierAtArrival = 0.0;
        int    actualContainerCountAtArrival = 0;
    };

    explicit PathComparisonViewModel(
        const QList<const PathData *> &paths);

    int maxSegments() const;
    int maxTerminals() const;

    QString pathLabel(const PathData *pathData) const;
    QString terminalDisplayName(const Backend::Path *path,
                                const QString       &terminalId) const;
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
    Backend::PathSegment::SegmentCostSnapshot segmentPredictedCosts(
        const PathData *pathData,
        int             segmentIndex) const;
    Backend::PathSegment::SegmentCostSnapshot segmentActualCosts(
        const PathData *pathData,
        int             segmentIndex) const;
    PathCostTotals pathCostTotals(
        const PathData *pathData) const;

    QString simulatedCostText(const PathData *pathData) const;
    QString simulatedEdgeCostText(const PathData *pathData) const;
    QString simulatedTerminalCostText(const PathData *pathData) const;
    QString costDifferenceText(const PathData *pathData) const;

private:
    static Backend::PathSegment::SegmentCostSnapshot
    sumCostSnapshots(const QList<Backend::PathSegment *> &segments,
                     bool useActualCosts);

    const QList<const PathData *> &m_paths;
};

} // namespace GUI
} // namespace CargoNetSim
