#include "ScenarioExecutionResult.h"

#include <QJsonValue>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/ConfigController.h"
#include "PathMetricsCalculator.h"
#include "PropertyKeys.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

namespace PK = PropertyKeys;

QString modeFieldDebugSummary(const QJsonObject &json,
                              const QString     &key)
{
    const bool contains = json.contains(key);
    const auto value    = json.value(key);
    return QStringLiteral("contains=%1 isDouble=%2 isString=%3 int=%4 string=\"%5\"")
        .arg(contains)
        .arg(value.isDouble())
        .arg(value.isString())
        .arg(value.toInt(-99999))
        .arg(value.toString());
}

QJsonObject metricSnapshotToJson(
    const PathSegment::SegmentMetricSnapshot &snapshot)
{
    QJsonObject json;
    json[QStringLiteral("available")] = snapshot.available;
    json[QStringLiteral("travel_time_seconds")] =
        snapshot.travelTime;
    json[QStringLiteral("distance_meters")] = snapshot.distance;
    json[QStringLiteral("carbon_emissions")] =
        snapshot.carbonEmissions;
    json[QStringLiteral("energy_consumption")] =
        snapshot.energyConsumption;
    json[QStringLiteral("risk")] = snapshot.risk;
    return json;
}

PathSegment::SegmentMetricSnapshot metricSnapshotFromJson(
    const QJsonObject &json)
{
    PathSegment::SegmentMetricSnapshot snapshot;
    snapshot.available =
        json.value(QStringLiteral("available")).toBool(false);
    snapshot.travelTime =
        json.value(QStringLiteral("travel_time_seconds"))
            .toDouble(0.0);
    snapshot.distance =
        json.value(QStringLiteral("distance_meters")).toDouble(0.0);
    snapshot.carbonEmissions =
        json.value(QStringLiteral("carbon_emissions"))
            .toDouble(0.0);
    snapshot.energyConsumption =
        json.value(QStringLiteral("energy_consumption"))
            .toDouble(0.0);
    snapshot.risk = json.value(QStringLiteral("risk")).toDouble(0.0);
    return snapshot;
}

QJsonObject costSnapshotToJson(
    const PathSegment::SegmentCostSnapshot &snapshot)
{
    QJsonObject json;
    json[QStringLiteral("available")] = snapshot.available;
    json[QStringLiteral("travel_time_cost")] =
        snapshot.travelTime;
    json[QStringLiteral("distance_cost")] = snapshot.distance;
    json[QStringLiteral("carbon_cost")] =
        snapshot.carbonEmissions;
    json[QStringLiteral("energy_cost")] =
        snapshot.energyConsumption;
    json[QStringLiteral("risk_cost")] = snapshot.risk;
    json[QStringLiteral("direct_cost")] = snapshot.directCost;
    return json;
}

PathSegment::SegmentCostSnapshot costSnapshotFromJson(
    const QJsonObject &json)
{
    PathSegment::SegmentCostSnapshot snapshot;
    snapshot.available =
        json.value(QStringLiteral("available")).toBool(false);
    snapshot.travelTime =
        json.value(QStringLiteral("travel_time_cost")).toDouble(0.0);
    snapshot.distance =
        json.value(QStringLiteral("distance_cost")).toDouble(0.0);
    snapshot.carbonEmissions =
        json.value(QStringLiteral("carbon_cost")).toDouble(0.0);
    snapshot.energyConsumption =
        json.value(QStringLiteral("energy_cost")).toDouble(0.0);
    snapshot.risk =
        json.value(QStringLiteral("risk_cost")).toDouble(0.0);
    snapshot.directCost =
        json.value(QStringLiteral("direct_cost")).toDouble(0.0);
    return snapshot;
}

int capacityForMode(ConfigController                         *config,
                    TransportationTypes::TransportationMode mode)
{
    if (!config)
        return 1;

    const QVariantMap modeConfig =
        config->getTransportModes()
            .value(transportationModeToString(mode))
            .toMap();
    return modeConfig
        .value(PK::Mode::AverageContainerNumber, 1)
        .toInt();
}

} // namespace

QJsonObject SegmentExecutionResult::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("segment_index")] = segmentIndex;
    json[QStringLiteral("segment_id")] = segmentId;
    json[QStringLiteral("start_terminal_id")] = startTerminalId;
    json[QStringLiteral("end_terminal_id")] = endTerminalId;
    json[QStringLiteral("mode")] =
        TransportationTypes::toInt(mode);
    json[QStringLiteral("actual_metrics")] =
        metricSnapshotToJson(actualMetrics);
    json[QStringLiteral("actual_costs")] =
        costSnapshotToJson(actualCosts);
    return json;
}

SegmentExecutionResult SegmentExecutionResult::fromJson(
    const QJsonObject &json)
{
    qCDebug(lcScenario)
        << "SegmentExecutionResult::fromJson:"
        << "segment_id="
        << json.value(QStringLiteral("segment_id")).toString()
        << "mode{"
        << modeFieldDebugSummary(json, QStringLiteral("mode"))
        << "}";
    SegmentExecutionResult result;
    result.segmentIndex =
        json.value(QStringLiteral("segment_index")).toInt(-1);
    result.segmentId =
        json.value(QStringLiteral("segment_id")).toString();
    result.startTerminalId =
        json.value(QStringLiteral("start_terminal_id")).toString();
    result.endTerminalId =
        json.value(QStringLiteral("end_terminal_id")).toString();
    result.mode = TransportationTypes::fromInt(
        json.value(QStringLiteral("mode")).toInt(-1));
    result.actualMetrics = metricSnapshotFromJson(
        json.value(QStringLiteral("actual_metrics")).toObject());
    result.actualCosts = costSnapshotFromJson(
        json.value(QStringLiteral("actual_costs")).toObject());
    qCDebug(lcScenario)
        << "SegmentExecutionResult::fromJson: decoded"
        << "segment_id=" << result.segmentId
        << "segment_index=" << result.segmentIndex
        << "mode=" << static_cast<int>(result.mode)
        << "start_terminal_id=" << result.startTerminalId
        << "end_terminal_id=" << result.endTerminalId;
    return result;
}

QJsonObject TerminalExecutionResult::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("execution_id")] = executionId;
    json[QStringLiteral("path_identity")] = pathIdentity;
    json[QStringLiteral("scenario_terminal_id")] =
        scenarioTerminalId;
    json[QStringLiteral("runtime_terminal_id")] =
        runtimeTerminalId;
    json[QStringLiteral("arrival_mode")] =
        TransportationTypes::toInt(arrivalMode);
    json[QStringLiteral("terminal_sequence_index")] =
        terminalSequenceIndex;
    json[QStringLiteral("total_dropped_containers")] =
        totalDroppedContainers;
    json[QStringLiteral("total_picked_containers")] =
        totalPickedContainers;
    json[QStringLiteral("arrival_events")] = arrivalEvents;
    json[QStringLiteral("pickup_events")] = pickupEvents;
    json[QStringLiteral("actual_yard_dwell_seconds")] =
        actualYardDwellSeconds;
    json[QStringLiteral("actual_customs_delay_seconds")] =
        actualCustomsDelaySeconds;
    json[QStringLiteral("customs_applied_count")] =
        customsAppliedCount;
    json[QStringLiteral("actual_arrival_penalty_seconds")] =
        actualArrivalPenaltySeconds;
    json[QStringLiteral("actual_total_handling_seconds")] =
        actualTotalHandlingSeconds;
    json[QStringLiteral("actual_direct_cost_usd")] =
        actualDirectCostUsd;
    json[QStringLiteral("actual_weighted_delay_contribution")] =
        actualWeightedDelayContribution;
    json[QStringLiteral("actual_weighted_cost_contribution")] =
        actualWeightedCostContribution;
    json[QStringLiteral("actual_weighted_total_contribution")] =
        actualWeightedTotalContribution;
    json[QStringLiteral("first_arrival_state_snapshot")] =
        firstArrivalStateSnapshot;
    json[QStringLiteral("last_departure_state_snapshot")] =
        lastDepartureStateSnapshot;
    json[QStringLiteral("raw_batch_records")] = rawBatchRecords;
    return json;
}

TerminalExecutionResult TerminalExecutionResult::fromJson(
    const QJsonObject &json)
{
    qCDebug(lcScenario)
        << "TerminalExecutionResult::fromJson:"
        << "path_identity="
        << json.value(QStringLiteral("path_identity")).toString()
        << "scenario_terminal_id="
        << json.value(QStringLiteral("scenario_terminal_id")).toString()
        << "arrival_mode{"
        << modeFieldDebugSummary(
               json, QStringLiteral("arrival_mode"))
        << "}"
        << "raw_batch_records="
        << json.value(QStringLiteral("raw_batch_records"))
               .toArray()
               .size();
    TerminalExecutionResult result;
    result.executionId =
        json.value(QStringLiteral("execution_id")).toString();
    result.pathIdentity =
        json.value(QStringLiteral("path_identity")).toString();
    result.scenarioTerminalId =
        json.value(QStringLiteral("scenario_terminal_id"))
            .toString();
    result.runtimeTerminalId =
        json.value(QStringLiteral("runtime_terminal_id"))
            .toString();
    result.arrivalMode = TransportationTypes::fromInt(
        json.value(QStringLiteral("arrival_mode")).toInt(-1));
    result.terminalSequenceIndex =
        json.value(QStringLiteral("terminal_sequence_index"))
            .toInt(-1);
    result.totalDroppedContainers =
        json.value(QStringLiteral("total_dropped_containers"))
            .toInt(0);
    result.totalPickedContainers =
        json.value(QStringLiteral("total_picked_containers"))
            .toInt(0);
    result.arrivalEvents =
        json.value(QStringLiteral("arrival_events")).toInt(0);
    result.pickupEvents =
        json.value(QStringLiteral("pickup_events")).toInt(0);
    result.actualYardDwellSeconds =
        json.value(QStringLiteral("actual_yard_dwell_seconds"))
            .toDouble(0.0);
    result.actualCustomsDelaySeconds =
        json.value(QStringLiteral("actual_customs_delay_seconds"))
            .toDouble(0.0);
    result.customsAppliedCount =
        json.value(QStringLiteral("customs_applied_count"))
            .toInt(0);
    result.actualArrivalPenaltySeconds =
        json.value(QStringLiteral("actual_arrival_penalty_seconds"))
            .toDouble(0.0);
    result.actualTotalHandlingSeconds =
        json.value(QStringLiteral("actual_total_handling_seconds"))
            .toDouble(0.0);
    result.actualDirectCostUsd =
        json.value(QStringLiteral("actual_direct_cost_usd"))
            .toDouble(0.0);
    result.actualWeightedDelayContribution =
        json.value(QStringLiteral("actual_weighted_delay_contribution"))
            .toDouble(0.0);
    result.actualWeightedCostContribution =
        json.value(QStringLiteral("actual_weighted_cost_contribution"))
            .toDouble(0.0);
    result.actualWeightedTotalContribution =
        json.value(QStringLiteral("actual_weighted_total_contribution"))
            .toDouble(0.0);
    result.firstArrivalStateSnapshot =
        json.value(QStringLiteral("first_arrival_state_snapshot"))
            .toObject();
    result.lastDepartureStateSnapshot =
        json.value(QStringLiteral("last_departure_state_snapshot"))
            .toObject();
    result.rawBatchRecords =
        json.value(QStringLiteral("raw_batch_records")).toArray();
    qCDebug(lcScenario)
        << "TerminalExecutionResult::fromJson: decoded"
        << "path_identity=" << result.pathIdentity
        << "scenario_terminal_id="
        << result.scenarioTerminalId
        << "runtime_terminal_id="
        << result.runtimeTerminalId
        << "arrivalMode=" << static_cast<int>(result.arrivalMode)
        << "terminal_sequence_index="
        << result.terminalSequenceIndex
        << "dropped=" << result.totalDroppedContainers
        << "picked=" << result.totalPickedContainers;
    return result;
}

PathSimulationResult PathExecutionResult::toSimulationResult() const
{
    PathSimulationResult result;
    result.pathId = pathId;
    result.canonicalPathKey = canonicalPathKey;
    result.pathUid = pathUid;
    result.originId = originId;
    result.destinationId = destinationId;
    result.rank = rank;
    result.effectiveContainerCount = effectiveContainerCount;
    result.totalCost = totalCost;
    result.edgeCosts = edgeCosts;
    result.terminalCosts = terminalCosts;
    return result;
}

PathSegment::SegmentMetricSnapshot
PathExecutionResult::totalActualMetrics() const
{
    PathSegment::SegmentMetricSnapshot total;
    for (const auto &segment : segmentResults)
    {
        if (!segment.actualMetrics.available)
            continue;

        total.available = true;
        total.travelTime += segment.actualMetrics.travelTime;
        total.distance += segment.actualMetrics.distance;
        total.carbonEmissions +=
            segment.actualMetrics.carbonEmissions;
        total.energyConsumption +=
            segment.actualMetrics.energyConsumption;
        total.risk += segment.actualMetrics.risk;
    }
    return total;
}

PathSegment::SegmentCostSnapshot
PathExecutionResult::totalActualCosts() const
{
    PathSegment::SegmentCostSnapshot total;
    for (const auto &segment : segmentResults)
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

PathMetrics PathExecutionResult::toActualMetrics(
    ConfigController *config) const
{
    PathMetrics metrics;
    const auto totals = totalActualMetrics();
    if (!totals.available)
        return metrics;

    metrics.valid = true;
    metrics.setDistance(
        Units::toKilometers(Units::meters(totals.distance)));
    metrics.setTravelTime(
        Units::toHours(Units::seconds(totals.travelTime)));
    metrics.setEnergyPerVehicle(
        Units::kilowattHours(totals.energyConsumption));
    metrics.setCarbonPerVehicle(
        Units::metricTons(totals.carbonEmissions));
    metrics.setRiskPerVehicle(Units::scalar(totals.risk));

    if (effectiveContainerCount > 0)
    {
        const auto mode = segmentResults.isEmpty()
            ? TransportationTypes::TransportationMode::Any
            : segmentResults.first().mode;
        PathMetricsCalculator::projectPerContainer(
            metrics, effectiveContainerCount,
            capacityForMode(config, mode));
    }

    return metrics;
}

QJsonObject PathExecutionResult::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("execution_id")] = executionId;
    json[QStringLiteral("path_identity")] = pathIdentity;
    json[QStringLiteral("path_id")] = pathId;
    json[QStringLiteral("canonical_path_key")] = canonicalPathKey;
    json[QStringLiteral("path_uid")] = pathUid;
    json[QStringLiteral("origin_id")] = originId;
    json[QStringLiteral("destination_id")] = destinationId;
    json[QStringLiteral("rank")] = rank;
    json[QStringLiteral("effective_container_count")] =
        effectiveContainerCount;
    json[QStringLiteral("total_cost")] = totalCost;
    json[QStringLiteral("edge_costs")] = edgeCosts;
    json[QStringLiteral("terminal_costs")] = terminalCosts;
    json[QStringLiteral("modeled_actual_terminal_costs")] =
        modeledActualTerminalCosts;

    QJsonArray segments;
    for (const auto &segment : segmentResults)
        segments.append(segment.toJson());
    json[QStringLiteral("segment_results")] = segments;

    QJsonArray terminals;
    for (const auto &terminal : terminalResults)
        terminals.append(terminal.toJson());
    json[QStringLiteral("terminal_results")] = terminals;
    return json;
}

PathExecutionResult PathExecutionResult::fromJson(
    const QJsonObject &json)
{
    qCDebug(lcScenario)
        << "PathExecutionResult::fromJson:"
        << "path_identity="
        << json.value(QStringLiteral("path_identity")).toString()
        << "path_id="
        << json.value(QStringLiteral("path_id")).toInt(-1)
        << "rank="
        << json.value(QStringLiteral("rank")).toInt(-1)
        << "segment_results="
        << json.value(QStringLiteral("segment_results"))
               .toArray()
               .size()
        << "terminal_results="
        << json.value(QStringLiteral("terminal_results"))
               .toArray()
               .size();
    PathExecutionResult result;
    result.executionId =
        json.value(QStringLiteral("execution_id")).toString();
    result.pathIdentity =
        json.value(QStringLiteral("path_identity")).toString();
    result.pathId =
        json.value(QStringLiteral("path_id")).toInt(-1);
    result.canonicalPathKey =
        json.value(QStringLiteral("canonical_path_key")).toString();
    result.pathUid =
        json.value(QStringLiteral("path_uid")).toString();
    result.originId =
        json.value(QStringLiteral("origin_id")).toString();
    result.destinationId =
        json.value(QStringLiteral("destination_id")).toString();
    result.rank = json.value(QStringLiteral("rank")).toInt(0);
    result.effectiveContainerCount =
        json.value(QStringLiteral("effective_container_count"))
            .toInt(0);
    result.totalCost =
        json.value(QStringLiteral("total_cost")).toDouble(0.0);
    result.edgeCosts =
        json.value(QStringLiteral("edge_costs")).toDouble(0.0);
    result.terminalCosts =
        json.value(QStringLiteral("terminal_costs")).toDouble(0.0);
    result.modeledActualTerminalCosts =
        json.value(QStringLiteral("modeled_actual_terminal_costs"))
            .toDouble(0.0);

    const QJsonArray segments =
        json.value(QStringLiteral("segment_results")).toArray();
    for (const auto &segmentValue : segments)
    {
        if (!segmentValue.isObject())
            continue;
        result.segmentResults.append(
            SegmentExecutionResult::fromJson(
                segmentValue.toObject()));
    }

    const QJsonArray terminals =
        json.value(QStringLiteral("terminal_results")).toArray();
    for (const auto &terminalValue : terminals)
    {
        if (!terminalValue.isObject())
            continue;
        result.terminalResults.append(
            TerminalExecutionResult::fromJson(
                terminalValue.toObject()));
    }
    qCDebug(lcScenario)
        << "PathExecutionResult::fromJson: decoded"
        << "path_identity=" << result.pathIdentity
        << "path_id=" << result.pathId
        << "rank=" << result.rank
        << "effective_container_count="
        << result.effectiveContainerCount
        << "segmentResults=" << result.segmentResults.size()
        << "terminalResults=" << result.terminalResults.size();
    return result;
}

int ScenarioExecutionResultSet::size() const
{
    return m_pathResults.size();
}

bool ScenarioExecutionResultSet::isEmpty() const
{
    return m_pathResults.isEmpty();
}

const QList<PathExecutionResult> &
ScenarioExecutionResultSet::pathResults() const
{
    return m_pathResults;
}

void ScenarioExecutionResultSet::addPathResult(
    const PathExecutionResult &result)
{
    for (auto it = m_pathResults.begin(); it != m_pathResults.end(); ++it)
    {
        if (it->pathIdentity == result.pathIdentity)
        {
            *it = result;
            return;
        }
    }
    m_pathResults.append(result);
}

const PathExecutionResult *
ScenarioExecutionResultSet::findByPathIdentity(
    const QString &pathIdentity) const
{
    for (int i = 0; i < m_pathResults.size(); ++i)
    {
        if (m_pathResults[i].pathIdentity == pathIdentity)
            return &m_pathResults[i];
    }
    return nullptr;
}

QList<PathSimulationResult>
ScenarioExecutionResultSet::summaryResults() const
{
    QList<PathSimulationResult> results;
    results.reserve(m_pathResults.size());
    for (const auto &result : m_pathResults)
        results.append(result.toSimulationResult());
    return results;
}

QHash<QString, PathMetrics>
ScenarioExecutionResultSet::actualMetricsByPathIdentity(
    ConfigController *config) const
{
    QHash<QString, PathMetrics> metrics;
    for (const auto &result : m_pathResults)
    {
        metrics.insert(result.pathIdentity,
                       result.toActualMetrics(config));
    }
    return metrics;
}

QJsonArray ScenarioExecutionResultSet::toJson() const
{
    QJsonArray array;
    for (const auto &result : m_pathResults)
        array.append(result.toJson());
    return array;
}

ScenarioExecutionResultSet ScenarioExecutionResultSet::fromJson(
    const QJsonArray &json)
{
    ScenarioExecutionResultSet results;
    qCInfo(lcScenario)
        << "ScenarioExecutionResultSet::fromJson:"
        << "path_result_records=" << json.size();
    for (const auto &value : json)
    {
        if (!value.isObject())
            continue;
        results.addPathResult(
            PathExecutionResult::fromJson(value.toObject()));
    }
    qCInfo(lcScenario)
        << "ScenarioExecutionResultSet::fromJson:"
        << "decoded_path_results=" << results.size();
    return results;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
