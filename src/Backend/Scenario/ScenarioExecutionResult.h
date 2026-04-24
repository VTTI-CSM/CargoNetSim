#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/PathSegment.h"
#include "PathMetrics.h"
#include "PathSimulationResult.h"

namespace CargoNetSim
{
namespace Backend
{
class ConfigController;

namespace Scenario
{

struct SegmentExecutionResult
{
    int segmentIndex = -1;
    QString segmentId;
    QString startTerminalId;
    QString endTerminalId;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Any;
    PathSegment::SegmentMetricSnapshot actualMetrics;
    PathSegment::SegmentCostSnapshot   actualCosts;

    QJsonObject toJson() const;
    static SegmentExecutionResult fromJson(const QJsonObject &json);
};

struct PathExecutionResult
{
    QString pathIdentity;
    int     pathId = -1;
    QString canonicalPathKey;
    QString pathUid;
    QString originId;
    QString destinationId;
    int     rank = 0;
    int     effectiveContainerCount = 0;
    double  totalCost = 0.0;
    double  edgeCosts = 0.0;
    double  terminalCosts = 0.0;
    QList<SegmentExecutionResult> segmentResults;

    PathSimulationResult toSimulationResult() const;
    PathSegment::SegmentMetricSnapshot totalActualMetrics() const;
    PathSegment::SegmentCostSnapshot   totalActualCosts() const;
    PathMetrics toActualMetrics(ConfigController *config) const;

    QJsonObject toJson() const;
    static PathExecutionResult fromJson(const QJsonObject &json);
};

class ScenarioExecutionResultSet
{
public:
    int  size() const;
    bool isEmpty() const;

    const QList<PathExecutionResult> &pathResults() const;
    void addPathResult(const PathExecutionResult &result);
    const PathExecutionResult *findByPathIdentity(
        const QString &pathIdentity) const;

    QList<PathSimulationResult> summaryResults() const;
    QHash<QString, PathMetrics> actualMetricsByPathIdentity(
        ConfigController *config) const;

    QJsonArray toJson() const;
    static ScenarioExecutionResultSet fromJson(const QJsonArray &json);

private:
    QList<PathExecutionResult> m_pathResults;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
