#include "PathPresentationService.h"

#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"
#include "Backend/Scenario/PathPreparationService.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/ScenarioDocument.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{
namespace PK = Scenario::PropertyKeys;

QJsonArray vehicleBreakdownToJson(
    const QList<Scenario::PathMetrics::VehicleRequirement>
        &requirements)
{
    QJsonArray json;
    for (const auto &requirement : requirements)
    {
        QJsonObject entry;
        entry[QStringLiteral("segment_index")] =
            requirement.segmentIndex;
        entry[QStringLiteral("mode")] =
            static_cast<int>(requirement.mode);
        entry[QStringLiteral("vehicles_needed")] =
            requirement.vehiclesNeeded;
        json.append(entry);
    }
    return json;
}

QJsonObject filteredSegmentAttributes(
    const QJsonObject &attributes)
{
    QJsonObject filtered = attributes;
    filtered.remove(PK::Segment::Estimated);
    filtered.remove(PK::Segment::EstimatedCost);
    filtered.remove(PK::Segment::ActualValues);
    filtered.remove(PK::Segment::ActualCost);
    return filtered;
}

QList<Scenario::PathMetrics::VehicleRequirement>
vehicleBreakdownFromJson(const QJsonValue &value)
{
    QList<Scenario::PathMetrics::VehicleRequirement>
        requirements;
    if (!value.isArray())
        return requirements;

    const auto json = value.toArray();
    requirements.reserve(json.size());
    for (const auto &entryValue : json)
    {
        if (!entryValue.isObject())
            continue;
        const auto entry = entryValue.toObject();
        Scenario::PathMetrics::VehicleRequirement requirement;
        requirement.segmentIndex =
            entry.value(QStringLiteral("segment_index"))
                .toInt(-1);
        requirement.mode = static_cast<
            TransportationTypes::TransportationMode>(
            entry.value(QStringLiteral("mode")).toInt(
                static_cast<int>(
                    TransportationTypes::TransportationMode::Any)));
        requirement.vehiclesNeeded =
            entry.value(QStringLiteral("vehicles_needed"))
                .toInt(0);
        requirements.append(requirement);
    }
    return requirements;
}

QJsonObject metricsToJson(const Scenario::PathMetrics &metrics)
{
    QJsonObject o;
    o[QStringLiteral("valid")] = metrics.valid;
    o[QStringLiteral("distance_km")] = metrics.distanceKm;
    o[QStringLiteral("travel_time_hours")] =
        metrics.travelTimeHours;
    o[QStringLiteral("risk_per_vehicle")] =
        metrics.riskPerVehicle;
    o[QStringLiteral("fuel_per_vehicle")] =
        metrics.fuelPerVehicle;
    o[QStringLiteral("energy_per_vehicle")] =
        metrics.energyPerVehicle;
    o[QStringLiteral("carbon_per_vehicle")] =
        metrics.carbonPerVehicle;
    o[QStringLiteral("fuel_type")] = metrics.fuelType;
    o[QStringLiteral("preview_container_count")] =
        metrics.containerCount;
    o[QStringLiteral("preview_vehicles_needed")] =
        metrics.vehiclesNeeded;
    o[QStringLiteral("preview_vehicle_breakdown")] =
        vehicleBreakdownToJson(
            metrics.previewVehicleBreakdown);
    o[QStringLiteral("container_count")] =
        metrics.containerCount;
    o[QStringLiteral("fuel_per_container")] =
        metrics.fuelPerContainer;
    o[QStringLiteral("energy_per_container")] =
        metrics.energyPerContainer;
    o[QStringLiteral("carbon_per_container")] =
        metrics.carbonPerContainer;
    o[QStringLiteral("risk_per_container")] =
        metrics.riskPerContainer;
    o[QStringLiteral("vehicles_needed")] =
        metrics.vehiclesNeeded;
    return o;
}

Scenario::PathMetrics metricsFromJson(const QJsonObject &o)
{
    Scenario::PathMetrics metrics;
    metrics.valid =
        o.value(QStringLiteral("valid")).toBool(false);
    metrics.setDistance(Units::kilometers(
        o.value(QStringLiteral("distance_km")).toDouble(0.0)));
    metrics.setTravelTime(Units::hours(
        o.value(QStringLiteral("travel_time_hours"))
            .toDouble(0.0)));
    metrics.setRiskPerVehicle(Units::scalar(
        o.value(QStringLiteral("risk_per_vehicle"))
            .toDouble(0.0)));
    metrics.fuelPerVehicle =
        o.value(QStringLiteral("fuel_per_vehicle"))
            .toDouble(0.0);
    metrics.setEnergyPerVehicle(Units::kilowattHours(
        o.value(QStringLiteral("energy_per_vehicle"))
            .toDouble(0.0)));
    metrics.setCarbonPerVehicle(Units::metricTons(
        o.value(QStringLiteral("carbon_per_vehicle"))
            .toDouble(0.0)));
    metrics.fuelType =
        o.value(QStringLiteral("fuel_type")).toString();
    metrics.containerCount =
        o.contains(QStringLiteral("preview_container_count"))
            ? o.value(QStringLiteral("preview_container_count"))
                  .toInt(0)
            : o.value(QStringLiteral("container_count"))
                  .toInt(0);
    metrics.fuelPerContainer =
        o.value(QStringLiteral("fuel_per_container"))
            .toDouble(0.0);
    metrics.energyPerContainer =
        o.value(QStringLiteral("energy_per_container"))
            .toDouble(0.0);
    metrics.carbonPerContainer =
        o.value(QStringLiteral("carbon_per_container"))
            .toDouble(0.0);
    metrics.riskPerContainer =
        o.value(QStringLiteral("risk_per_container"))
            .toDouble(0.0);
    metrics.vehiclesNeeded =
        o.contains(QStringLiteral("preview_vehicles_needed"))
            ? o.value(QStringLiteral("preview_vehicles_needed"))
                  .toInt(0)
            : o.value(QStringLiteral("vehicles_needed"))
                  .toInt(0);
    metrics.previewVehicleBreakdown =
        vehicleBreakdownFromJson(
            o.value(QStringLiteral("preview_vehicle_breakdown")));
    return metrics;
}

QString sha256Hex(const QJsonObject &o)
{
    const QByteArray payload =
        QJsonDocument(o).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(
        QCryptographicHash::hash(
            payload, QCryptographicHash::Sha256)
            .toHex());
}

QJsonObject buildPathSnapshotJson(const Backend::Path &path)
{
    QJsonObject pathJson;
    pathJson[QStringLiteral("path_id")] = path.getPathId();
    pathJson[QStringLiteral("path_uid")] = path.getPathUid();
    pathJson[QStringLiteral("canonical_path_key")] =
        path.canonicalPathKey();
    pathJson[QStringLiteral("total_path_cost")] =
        path.getTotalPathCost();
    pathJson[QStringLiteral("total_edge_costs")] =
        path.getTotalEdgeCosts();
    pathJson[QStringLiteral("total_terminal_costs")] =
        path.getTotalTerminalCosts();
    pathJson[QStringLiteral("ranking_cost")] =
        path.getRankingCost();
    pathJson[QStringLiteral("start_terminal")] =
        path.getOriginId();
    pathJson[QStringLiteral("end_terminal")] =
        path.getDestinationId();
    pathJson[QStringLiteral("rank")] = path.getRank();
    pathJson[QStringLiteral("requested_mode")] =
        path.getRequestedMode();
    pathJson[QStringLiteral("requested_top_n")] =
        path.getRequestedTopN();
    pathJson[QStringLiteral(
        "skip_same_mode_terminal_delays_and_costs")] =
        path.skipSameModeTerminalDelaysAndCosts();
    pathJson[QStringLiteral("effective_container_count")] =
        path.getEffectiveContainerCount();
    pathJson[QStringLiteral("weighted_terminal_delay_total")] =
        path.getWeightedTerminalDelayTotal();
    pathJson[QStringLiteral(
        "weighted_terminal_direct_cost_total")] =
        path.getWeightedTerminalDirectCostTotal();
    pathJson[QStringLiteral("raw_terminal_delay_total")] =
        path.getRawTerminalDelayTotal();
    pathJson[QStringLiteral("raw_terminal_cost_total")] =
        path.getRawTerminalCostTotal();
    if (!path.getCostBreakdown().isEmpty())
        pathJson[QStringLiteral("cost_breakdown")] =
            path.getCostBreakdown();

    QJsonObject discoveryContext;
    discoveryContext[QStringLiteral("start_terminal")] =
        path.getOriginId();
    discoveryContext[QStringLiteral("end_terminal")] =
        path.getDestinationId();
    discoveryContext[QStringLiteral("requested_mode")] =
        path.getRequestedMode();
    discoveryContext[QStringLiteral("requested_top_n")] =
        path.getRequestedTopN();
    discoveryContext[QStringLiteral(
        "skip_same_mode_terminal_delays_and_costs")] =
        path.skipSameModeTerminalDelaysAndCosts();
    pathJson[QStringLiteral("discovery_context")] =
        discoveryContext;

    QJsonArray terminals;
    for (const auto &terminal : path.getTerminalsInPath())
    {
        QJsonObject o;
        o[QStringLiteral("terminal")] = terminal.id;
        o[QStringLiteral("display_name")] = terminal.displayName;
        o[QStringLiteral("canonical_name")] =
            terminal.canonicalName;
        o[QStringLiteral("sequence_index")] =
            terminal.sequenceIndex;
        o[QStringLiteral("handling_time")] =
            terminal.handlingTime;
        o[QStringLiteral("cost")] = terminal.rawCost;
        o[QStringLiteral("costs_skipped")] =
            terminal.costsSkipped;
        o[QStringLiteral(
            "weighted_terminal_delay_contribution")] =
            terminal.weightedTerminalDelayContribution;
        o[QStringLiteral(
            "weighted_terminal_cost_contribution")] =
            terminal.weightedTerminalCostContribution;
        o[QStringLiteral(
            "weighted_terminal_total_contribution")] =
            terminal.weightedTerminalTotalContribution;
        if (!terminal.skipReason.isEmpty())
            o[QStringLiteral("skip_reason")] =
                terminal.skipReason;
        terminals.append(o);
    }
    pathJson[QStringLiteral("terminals_in_path")] = terminals;

    QJsonArray segments;
    for (const auto *segment : path.getSegments())
    {
        if (!segment)
            continue;
        QJsonObject o;
        o[QStringLiteral("from")] = segment->getStart();
        o[QStringLiteral("to")] = segment->getEnd();
        o[QStringLiteral("mode")] =
            TransportationTypes::toInt(segment->getMode());
        o[QStringLiteral("sequence_index")] =
            segment->sequenceIndex();
        o[QStringLiteral("ranking_cost_contribution")] =
            segment->rankingCostContribution();
        o[QStringLiteral("weighted_edge_cost")] =
            segment->weightedEdgeCost();
        o[QStringLiteral(
            "weighted_terminal_cost_embedded_in_segment")] =
            segment->weightedTerminalCostEmbeddedInSegment();
        o[QStringLiteral("attributes")] =
            segment->getAttributes();
        segments.append(o);
    }
    pathJson[QStringLiteral("segments")] = segments;
    return pathJson;
}

QString snapshotExecutionPathKey(const QJsonObject &snapshot,
                             const Backend::Path &path)
{
    const QString executionPathKey =
        snapshot.value(QStringLiteral("execution_path_key"))
            .toString();
    if (!executionPathKey.isEmpty())
        return executionPathKey;

    const QString topLevelKey =
        snapshot.value(QStringLiteral("canonical_path_key"))
            .toString();
    if (!topLevelKey.isEmpty())
        return topLevelKey;

    const QString pathUid =
        snapshot.value(QStringLiteral("path_uid")).toString();
    if (!pathUid.isEmpty())
        return pathUid;

    const QString canonicalKey = path.canonicalPathKey();
    if (!canonicalKey.isEmpty())
        return canonicalKey;

    return QStringLiteral("path_id|%1")
        .arg(path.getPathId());
}

QString baseExecutionPathKey(const Backend::Path &path)
{
    const QString canonicalKey = path.canonicalPathKey();
    if (!canonicalKey.isEmpty())
        return canonicalKey;

    return QStringLiteral("path_id|%1")
        .arg(path.getPathId());
}

const Scenario::PathExecutionResult *executionResultFor(
    const PathPresentationRecord &pathData)
{
    if (!pathData.executionResult.has_value())
        return nullptr;
    return &pathData.executionResult.value();
}

PathPresentationCostSnapshot costSnapshotFromSegmentCost(
    const Backend::PathSegment::SegmentCostSnapshot &snapshot)
{
    PathPresentationCostSnapshot out;
    out.available = snapshot.available;
    out.travelTime = snapshot.travelTime;
    out.distance = snapshot.distance;
    out.carbonEmissions = snapshot.carbonEmissions;
    out.energyConsumption = snapshot.energyConsumption;
    out.risk = snapshot.risk;
    out.directCost = snapshot.directCost;
    return out;
}

Backend::PathSegment::SegmentMetricSnapshot actualMetricsForSegment(
    const PathPresentationRecord &pathData, int segmentIndex)
{
    if (const auto *result = executionResultFor(pathData))
    {
        if (segmentIndex >= 0
            && segmentIndex < result->segmentResults.size())
        {
            return result->segmentResults[segmentIndex]
                .actualMetrics;
        }
    }

    const auto *path = pathData.path.get();
    if (!path)
        return {};

    const auto &segments = path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return segments[segmentIndex]->actualValues();
}

Backend::PathSegment::SegmentCostSnapshot actualCostsForSegment(
    const PathPresentationRecord &pathData, int segmentIndex)
{
    if (const auto *result = executionResultFor(pathData))
    {
        if (segmentIndex >= 0
            && segmentIndex < result->segmentResults.size())
        {
            return result->segmentResults[segmentIndex]
                .actualCosts;
        }
    }

    const auto *path = pathData.path.get();
    if (!path)
        return {};

    const auto &segments = path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return segments[segmentIndex]->actualCosts();
}

const Scenario::TerminalExecutionResult *terminalResultFor(
    const PathPresentationRecord &pathData, int terminalIndex)
{
    const auto *path = pathData.path.get();
    const auto *result = executionResultFor(pathData);
    if (!path || !result)
        return nullptr;

    const auto &terminals = path->getTerminalsInPath();
    if (terminalIndex < 0 || terminalIndex >= terminals.size())
        return nullptr;

    const auto &terminal = terminals[terminalIndex];
    const int sequenceIndex =
        terminal.sequenceIndex >= 0 ? terminal.sequenceIndex
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

PathPresentationCostSnapshot sumExecutionCosts(
    const QList<Scenario::SegmentExecutionResult> &segments)
{
    PathPresentationCostSnapshot total;
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

PathPresentationCostSnapshot sumCostSnapshots(
    const QList<Backend::PathSegment *> &segments,
    bool useActualCosts)
{
    PathPresentationCostSnapshot total;
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

} // namespace

QList<PathPresentationRecord>
PathPresentationService::recordsFromRawPaths(
    const QList<Backend::Path *> &paths,
    const QHash<QString, Scenario::PathMetrics> &predicted,
    const QHash<QString, Scenario::PathMetrics> &actual) const
{
    QVector<Backend::Path *> sorted(paths.begin(), paths.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Backend::Path *a,
                 const Backend::Path *b) {
                  if (!a)
                      return false;
                  if (!b)
                      return true;
                  return a->getTotalPathCost()
                         < b->getTotalPathCost();
              });

    QList<PathPresentationRecord> records;
    records.reserve(sorted.size());
    for (auto *path : sorted)
    {
        if (!path)
            continue;

        PathPresentationRecord record;
        record.path = std::shared_ptr<Backend::Path>(path);
        record.executionPathKey = baseExecutionPathKey(*path);
        record.pathKey = record.executionPathKey;
        record.predictedMetrics =
            predicted.value(record.executionPathKey);
        record.actualMetrics =
            actual.value(record.executionPathKey);
        records.append(std::move(record));
    }

    return records;
}

QList<PathPresentationRecord>
PathPresentationService::recordsFromPreparedPaths(
    const Scenario::PreparedPathSet &prepared,
    const QHash<QString, Scenario::PathMetrics> &actual,
    const QHash<QString, Scenario::PreparedPathEligibility>
        &eligibility) const
{
    QList<PathPresentationRecord> records;
    records.reserve(static_cast<qsizetype>(prepared.records().size()));

    for (const auto &preparedRecord : prepared.records())
    {
        if (!preparedRecord.path)
            continue;

        PathPresentationRecord record;
        record.path = preparedRecord.path;
        record.executionPathKey =
            preparedRecord.executionPathKey.isEmpty()
                ? baseExecutionPathKey(*preparedRecord.path)
                : preparedRecord.executionPathKey;
        record.pathKey = record.executionPathKey;
        record.eligibility =
            eligibility.value(preparedRecord.executionPathKey,
                              Scenario::PreparedPathEligibility{});
        record.predictedMetrics = preparedRecord.predictedMetrics;
        record.actualMetrics =
            actual.value(preparedRecord.executionPathKey);
        records.append(std::move(record));
    }

    return records;
}

QList<QJsonObject> PathPresentationService::buildComparisonSnapshots(
    const Scenario::ScenarioDocument    &doc,
    const QList<PathPresentationRecord> &paths,
    const QVariantMap                   &costWeights) const
{
    QList<QJsonObject> snapshots;

    QJsonObject topology;
    topology[QStringLiteral("regions")] = doc.regions.size();
    topology[QStringLiteral("terminals")] =
        doc.terminals.size();
    topology[QStringLiteral("linkages")] =
        doc.linkages.size();
    topology[QStringLiteral("connections")] =
        doc.connections.size();
    topology[QStringLiteral("global_links")] =
        doc.globalLinks.size();

    const QString discoveredAtUtc =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonObject scenarioSnapshot =
        ScenarioPersistenceService().toJson(doc);
    scenarioSnapshot.remove(
        QStringLiteral("comparison_snapshots"));
    const QString scenarioFingerprint =
        sha256Hex(scenarioSnapshot);

    const QString topologyFingerprint = sha256Hex(topology);
    const QString costFingerprint =
        sha256Hex(QJsonObject::fromVariantMap(costWeights));

    for (const auto &record : paths)
    {
        if (!record.path)
            continue;

        QJsonObject snapshot;
        snapshot[QStringLiteral("schema_version")] = 3;
        snapshot[QStringLiteral("execution_path_key")] =
            record.executionPathKey;
        snapshot[QStringLiteral("canonical_path_key")] =
            record.path->canonicalPathKey();
        snapshot[QStringLiteral("path_uid")] =
            record.path->getPathUid();

        QJsonObject selection;
        selection[QStringLiteral("is_visible")] =
            record.isVisible;
        selection[QStringLiteral("is_selected")] =
            record.isSelected;
        snapshot[QStringLiteral("selection")] = selection;

        QJsonObject queryContext;
        queryContext[QStringLiteral("origin")] =
            record.path->getOriginId();
        queryContext[QStringLiteral("destination")] =
            record.path->getDestinationId();
        queryContext[QStringLiteral("rank")] =
            record.path->getRank();
        queryContext[QStringLiteral("requested_mode")] =
            record.path->getRequestedMode();
        queryContext[QStringLiteral("requested_top_n")] =
            record.path->getRequestedTopN();
        queryContext[QStringLiteral(
            "skip_same_mode_terminal_delays_and_costs")] =
            record.path->skipSameModeTerminalDelaysAndCosts();
        snapshot[QStringLiteral("query_context")] =
            queryContext;

        QJsonObject provenance;
        provenance[QStringLiteral("discovered_at")] =
            discoveredAtUtc;
        provenance[QStringLiteral("scenario_fingerprint")] =
            scenarioFingerprint;
        provenance[QStringLiteral("topology_fingerprint")] =
            topologyFingerprint;
        provenance[QStringLiteral("cost_weights_fingerprint")] =
            costFingerprint;
        provenance[QStringLiteral("cost_weights")] =
            QJsonObject::fromVariantMap(costWeights);
        snapshot[QStringLiteral("discovery_provenance")] =
            provenance;

        snapshot[QStringLiteral("predicted_metrics")] =
            metricsToJson(record.predictedMetrics);
        snapshot[QStringLiteral("actual_metrics")] =
            metricsToJson(record.actualMetrics);

        QJsonObject simulation;
        simulation[QStringLiteral("state")] =
            record.simulationTotalCost >= 0.0
                ? QStringLiteral("simulated")
                : QStringLiteral("not_simulated");
        simulation[QStringLiteral("total_cost")] =
            record.simulationTotalCost;
        simulation[QStringLiteral("edge_costs")] =
            record.simulationEdgeCosts;
        simulation[QStringLiteral("terminal_costs")] =
            record.simulationTerminalCosts;
        simulation[QStringLiteral("effective_container_count")] =
            record.executionResult.has_value()
                ? record.executionResult
                      ->effectiveContainerCount
                : record.path->getEffectiveContainerCount();
        snapshot[QStringLiteral("simulation")] =
            simulation;
        if (record.executionResult.has_value())
        {
            snapshot[QStringLiteral("execution_result")] =
                record.executionResult->toJson();
        }

        snapshot[QStringLiteral("path")] =
            buildPathSnapshotJson(*record.path);
        snapshots.append(snapshot);
    }

    return snapshots;
}

QList<PathPresentationRecord>
PathPresentationService::loadComparisonSnapshots(
    const QList<QJsonObject> &snapshots) const
{
    QList<PathPresentationRecord> records;
    records.reserve(snapshots.size());

    for (const auto &snapshot : snapshots)
    {
        const QJsonObject pathJson =
            snapshot.value(QStringLiteral("path")).toObject();
        if (pathJson.isEmpty())
            continue;

        auto *path = Backend::Path::fromJson(pathJson, {});
        if (!path)
            continue;

        PathPresentationRecord record;
        record.path =
            std::shared_ptr<Backend::Path>(path);
        record.executionPathKey =
            snapshotExecutionPathKey(snapshot, *path);
        record.pathKey = record.executionPathKey;
        record.predictedMetrics = metricsFromJson(
            snapshot.value(QStringLiteral("predicted_metrics"))
                .toObject());
        record.actualMetrics = metricsFromJson(
            snapshot.value(QStringLiteral("actual_metrics"))
                .toObject());

        const QJsonObject simulation =
            snapshot.value(QStringLiteral("simulation"))
                .toObject();
        record.simulationTotalCost =
            simulation.value(QStringLiteral("total_cost"))
                .toDouble(-1.0);
        record.simulationEdgeCosts =
            simulation.value(QStringLiteral("edge_costs"))
                .toDouble(-1.0);
        record.simulationTerminalCosts =
            simulation.value(QStringLiteral("terminal_costs"))
                .toDouble(-1.0);

        const QJsonObject selection =
            snapshot.value(QStringLiteral("selection"))
                .toObject();
        record.isVisible =
            selection.value(QStringLiteral("is_visible"))
                .toBool(true);
        record.isSelected =
            selection.value(QStringLiteral("is_selected"))
                .toBool(false);

        const QJsonObject executionResult =
            snapshot.value(QStringLiteral("execution_result"))
                .toObject();
        if (!executionResult.isEmpty())
        {
            record.executionResult =
                Scenario::PathExecutionResult::fromJson(
                    executionResult);
        }

        records.append(std::move(record));
    }

    return records;
}

int PathPresentationService::maxSegments(
    const QList<const PathPresentationRecord *> &paths) const
{
    int maxSegments = 0;
    for (const auto *pathData : paths)
    {
        if (!pathData || !pathData->path)
            continue;
        maxSegments = qMax(maxSegments,
                           pathData->path->getSegments().size());
    }
    return maxSegments;
}

int PathPresentationService::maxTerminals(
    const QList<const PathPresentationRecord *> &paths) const
{
    int maxTerminals = 0;
    for (const auto *pathData : paths)
    {
        if (!pathData || !pathData->path)
            continue;
        maxTerminals = qMax(
            maxTerminals,
            pathData->path->getTerminalsInPath().size());
    }
    return maxTerminals;
}

PathPresentationSummary PathPresentationService::summary(
    const PathPresentationRecord &path) const
{
    PathPresentationSummary out;
    if (!path.path)
    {
        out.pathLabel = QObject::tr("Unknown Path");
        out.startTerminalName = QObject::tr("Unknown");
        out.endTerminalName = QObject::tr("Unknown");
        return out;
    }

    out.pathId = path.path->getPathId();
    out.pathLabel =
        QObject::tr("Path %1").arg(out.pathId);
    out.terminalCount =
        path.path->getTerminalsInPath().size();
    out.segmentCount = path.path->getSegments().size();
    out.predictedTotalCost =
        path.path->getTotalPathCost();
    out.predictedEdgeCost =
        path.path->getTotalEdgeCosts();
    out.predictedTerminalCost =
        path.path->getTotalTerminalCosts();
    out.startTerminalName =
        terminalDisplayName(path, path.path->getStartTerminal());
    out.endTerminalName =
        terminalDisplayName(path, path.path->getEndTerminal());
    return out;
}

QList<PathPresentationTerminalEntry>
PathPresentationService::terminalEntries(
    const PathPresentationRecord &path) const
{
    QList<PathPresentationTerminalEntry> entries;
    if (!path.path)
        return entries;

    const auto &terminals = path.path->getTerminalsInPath();
    entries.reserve(terminals.size());
    for (int i = 0; i < terminals.size(); ++i)
    {
        const auto &terminal = terminals[i];
        PathPresentationTerminalEntry entry;
        entry.index = i;
        entry.id = terminal.id;
        entry.displayName = terminal.displayName;
        entry.canonicalName = terminal.canonicalName;
        entry.predictedCostsSkipped = terminal.costsSkipped;
        entry.predictedSkipReason = terminal.skipReason;
        entry.displayValues = terminalValues(path, i);
        entries.append(std::move(entry));
    }

    return entries;
}

QList<PathPresentationSegmentEntry>
PathPresentationService::segmentEntries(
    const PathPresentationRecord &path) const
{
    QList<PathPresentationSegmentEntry> entries;
    if (!path.path)
        return entries;

    const auto &segments = path.path->getSegments();
    entries.reserve(segments.size());
    for (int i = 0; i < segments.size(); ++i)
    {
        const auto *segment = segments[i];
        if (!segment)
            continue;

        PathPresentationSegmentEntry entry;
        entry.index = i;
        entry.startTerminalId = segment->getStart();
        entry.endTerminalId = segment->getEnd();
        entry.startTerminalName =
            terminalDisplayName(path, entry.startTerminalId);
        entry.endTerminalName =
            terminalDisplayName(path, entry.endTerminalId);
        entry.mode = segment->getMode();
        entry.modeName =
            TransportationTypes::toString(segment->getMode());
        entry.description = QStringLiteral("%1 → %2 (%3)")
                                .arg(entry.startTerminalName,
                                     entry.endTerminalName,
                                     entry.modeName);
        entry.additionalAttributes =
            filteredSegmentAttributes(
                segment->getAttributes());
        entry.displayValues = segmentValues(path, i);
        entry.predictedCosts = segmentPredictedCosts(path, i);
        entry.actualCosts = segmentActualCosts(path, i);
        entries.append(std::move(entry));
    }

    return entries;
}

QString PathPresentationService::pathLabel(
    const PathPresentationRecord &path) const
{
    return summary(path).pathLabel;
}

QString PathPresentationService::terminalDisplayName(
    const PathPresentationRecord &path,
    const QString                &terminalId) const
{
    if (!path.path)
        return QObject::tr("Unknown");

    for (const auto &terminal : path.path->getTerminalsInPath())
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

    return terminalId.isEmpty() ? QObject::tr("Unknown")
                                : terminalId;
}

QString PathPresentationService::terminalNameAt(
    const PathPresentationRecord &path, int terminalIndex) const
{
    if (!path.path)
        return QObject::tr("N/A");

    const auto &terminals = path.path->getTerminalsInPath();
    if (terminalIndex < 0 || terminalIndex >= terminals.size())
        return QStringLiteral("-");

    const auto &terminal = terminals[terminalIndex];
    if (!terminal.displayName.isEmpty())
        return terminal.displayName;
    if (!terminal.canonicalName.isEmpty())
        return terminal.canonicalName;
    return terminal.id.isEmpty() ? QStringLiteral("-")
                                 : terminal.id;
}

PathPresentationTerminalDisplayValues
PathPresentationService::terminalValues(
    const PathPresentationRecord &path,
    int terminalIndex) const
{
    PathPresentationTerminalDisplayValues out;
    if (!path.path)
        return out;

    const auto &terminals = path.path->getTerminalsInPath();
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

    const auto *actual = terminalResultFor(path, terminalIndex);
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

    const auto state = actual->firstArrivalStateSnapshot
                           .value(QStringLiteral("state"))
                           .toObject();
    out.actualUtilizationAtArrival =
        state.value(QStringLiteral("utilization"))
            .toDouble(0.0);
    out.actualCongestionAtArrival =
        state.value(QStringLiteral("congestion"))
            .toDouble(0.0);
    out.actualDelayMultiplierAtArrival =
        state.value(QStringLiteral("delay_multiplier"))
            .toDouble(0.0);
    out.actualContainerCountAtArrival =
        actual->firstArrivalStateSnapshot
            .value(QStringLiteral("container_count"))
            .toInt(0);
    return out;
}

QString PathPresentationService::segmentDescription(
    const PathPresentationRecord &path, int segmentIndex) const
{
    if (!path.path)
        return QObject::tr("N/A");

    const auto &segments = path.path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return QStringLiteral("-");
    }

    const auto *segment = segments[segmentIndex];
    return QStringLiteral("%1 → %2 (%3)")
        .arg(terminalDisplayName(path, segment->getStart()),
             terminalDisplayName(path, segment->getEnd()),
             TransportationTypes::toString(segment->getMode()));
}

PathPresentationSegmentDisplayValues
PathPresentationService::segmentValues(
    const PathPresentationRecord &path, int segmentIndex) const
{
    PathPresentationSegmentDisplayValues out;
    if (!path.path)
        return out;

    const auto &segments = path.path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return out;
    }

    const auto *segment = segments[segmentIndex];
    const auto predicted = segment->estimatedValues();
    out.predictedAvailable = predicted.available;
    out.predictedDistanceKm =
        Units::LengthMeters(predicted.distance)
            .convert<units::length::kilometer>()
            .value();
    out.predictedTravelTimeHours =
        Units::TimeSeconds(predicted.travelTime)
            .convert<units::time::hour>()
            .value();
    out.predictedCarbonEmissions =
        predicted.carbonEmissions;
    out.predictedEnergyConsumption =
        predicted.energyConsumption;
    out.predictedRisk = predicted.risk;

    const auto actual =
        actualMetricsForSegment(path, segmentIndex);
    out.actualAvailable = actual.available;
    out.actualDistanceKm =
        Units::LengthMeters(actual.distance)
            .convert<units::length::kilometer>()
            .value();
    out.actualTravelTimeHours =
        Units::TimeSeconds(actual.travelTime)
            .convert<units::time::hour>()
            .value();
    out.actualCarbonEmissions =
        actual.carbonEmissions;
    out.actualEnergyConsumption =
        actual.energyConsumption;
    out.actualRisk = actual.risk;
    return out;
}

PathPresentationCostSnapshot
PathPresentationService::segmentPredictedCosts(
    const PathPresentationRecord &path, int segmentIndex) const
{
    if (!path.path)
        return {};

    const auto &segments = path.path->getSegments();
    if (segmentIndex < 0 || segmentIndex >= segments.size()
        || !segments[segmentIndex])
    {
        return {};
    }
    return costSnapshotFromSegmentCost(
        segments[segmentIndex]->estimatedCosts());
}

PathPresentationCostSnapshot
PathPresentationService::segmentActualCosts(
    const PathPresentationRecord &path, int segmentIndex) const
{
    return costSnapshotFromSegmentCost(
        actualCostsForSegment(path, segmentIndex));
}

PathPresentationCostTotals
PathPresentationService::pathCostTotals(
    const PathPresentationRecord &path) const
{
    PathPresentationCostTotals out;
    if (!path.path)
        return out;

    const auto segments = path.path->getSegments();
    out.predicted = sumCostSnapshots(segments, false);
    if (const auto *result = executionResultFor(path))
        out.actual = sumExecutionCosts(result->segmentResults);
    else
        out.actual = sumCostSnapshots(segments, true);
    return out;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
