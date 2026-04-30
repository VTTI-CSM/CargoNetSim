#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

#include <memory>
#include <optional>

#include "Backend/Models/Path.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/PreparedPathStatus.h"
#include "Backend/Scenario/ScenarioExecutionResult.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
class PreparedPathSet;
}

namespace Application
{

struct PathPresentationRecord
{
    QString                        executionPathKey;
    QString                        pathKey;
    std::shared_ptr<Backend::Path> path;
    Scenario::PreparedPathEligibility eligibility;
    Scenario::PathMetrics          predictedMetrics;
    Scenario::PathMetrics          actualMetrics;
    double                         simulationTotalCost = -1.0;
    double                         simulationEdgeCosts = -1.0;
    double                         simulationTerminalCosts = -1.0;
    bool                           isVisible = true;
    bool                           isSelected = false;
    std::optional<Scenario::PathExecutionResult>
        executionResult;

    bool hasSimulationTotalCost() const
    {
        return simulationTotalCost >= 0.0;
    }

    bool hasSimulationEdgeCosts() const
    {
        return simulationEdgeCosts >= 0.0;
    }

    bool hasSimulationTerminalCosts() const
    {
        return simulationTerminalCosts >= 0.0;
    }
};

struct PathPresentationCostSnapshot
{
    bool   available = false;
    double travelTime = 0.0;
    double distance = 0.0;
    double carbonEmissions = 0.0;
    double energyConsumption = 0.0;
    double risk = 0.0;
    double directCost = 0.0;
};

struct PathPresentationSegmentDisplayValues
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

struct PathPresentationTerminalDisplayValues
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
    TransportationTypes::TransportationMode actualArrivalMode =
        TransportationTypes::TransportationMode::Any;
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

struct PathPresentationCostTotals
{
    PathPresentationCostSnapshot predicted;
    PathPresentationCostSnapshot actual;
};

struct PathPresentationSummary
{
    int     pathId = -1;
    QString pathLabel;
    int     terminalCount = 0;
    int     segmentCount = 0;
    double  predictedTotalCost = 0.0;
    double  predictedEdgeCost = 0.0;
    double  predictedTerminalCost = 0.0;
    QString startTerminalName;
    QString endTerminalName;
};

struct PathPresentationTerminalEntry
{
    int     index = -1;
    QString id;
    QString displayName;
    QString canonicalName;
    bool    predictedCostsSkipped = false;
    QString predictedSkipReason;
    PathPresentationTerminalDisplayValues displayValues;
};

struct PathPresentationSegmentEntry
{
    int     index = -1;
    QString startTerminalId;
    QString endTerminalId;
    QString startTerminalName;
    QString endTerminalName;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Any;
    QString modeName;
    QString description;
    QJsonObject additionalAttributes;
    PathPresentationSegmentDisplayValues displayValues;
    PathPresentationCostSnapshot predictedCosts;
    PathPresentationCostSnapshot actualCosts;
};

class PathPresentationService
{
public:
    PathPresentationService() = default;

    QList<PathPresentationRecord> recordsFromRawPaths(
        const QList<Backend::Path *>        &paths,
        const QHash<QString, Scenario::PathMetrics> &predicted = {},
        const QHash<QString, Scenario::PathMetrics> &actual = {}) const;

    QList<PathPresentationRecord> recordsFromPreparedPaths(
        const Scenario::PreparedPathSet     &prepared,
        const QHash<QString, Scenario::PathMetrics> &actual = {},
        const QHash<QString, Scenario::PreparedPathEligibility>
            &eligibility = {}) const;

    QList<QJsonObject> buildComparisonSnapshots(
        const Scenario::ScenarioDocument          &doc,
        const QList<PathPresentationRecord>       &paths,
        const QVariantMap                         &costWeights) const;

    QList<PathPresentationRecord>
    loadComparisonSnapshots(
        const QList<QJsonObject> &snapshots) const;

    int maxSegments(
        const QList<const PathPresentationRecord *> &paths) const;
    int maxTerminals(
        const QList<const PathPresentationRecord *> &paths) const;

    PathPresentationSummary summary(
        const PathPresentationRecord &path) const;
    QList<PathPresentationTerminalEntry> terminalEntries(
        const PathPresentationRecord &path) const;
    QList<PathPresentationSegmentEntry> segmentEntries(
        const PathPresentationRecord &path) const;
    QString pathLabel(const PathPresentationRecord &path) const;
    QString terminalDisplayName(
        const PathPresentationRecord &path,
        const QString                &terminalId) const;
    QString terminalNameAt(const PathPresentationRecord &path,
                           int terminalIndex) const;
    PathPresentationTerminalDisplayValues terminalValues(
        const PathPresentationRecord &path,
        int terminalIndex) const;
    QString segmentDescription(const PathPresentationRecord &path,
                               int segmentIndex) const;
    PathPresentationSegmentDisplayValues segmentValues(
        const PathPresentationRecord &path,
        int segmentIndex) const;
    PathPresentationCostSnapshot segmentPredictedCosts(
        const PathPresentationRecord &path,
        int segmentIndex) const;
    PathPresentationCostSnapshot segmentActualCosts(
        const PathPresentationRecord &path,
        int segmentIndex) const;
    PathPresentationCostTotals pathCostTotals(
        const PathPresentationRecord &path) const;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
