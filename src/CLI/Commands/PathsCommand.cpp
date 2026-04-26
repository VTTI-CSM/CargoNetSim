#include "PathsCommand.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLocale>
#include <QScopeGuard>
#include <QStringList>

#include <cstdio>

#include "Backend/Application/PreparedPathService.h"
#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/CliApi/PathsApi.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

namespace {

struct Options
{
    QString scenarioPath;
    bool    json = false;
    bool    details = false;
    bool    hasTopOverride = false;
    int     topOverride = 0;
};

struct TableColumn
{
    QString header;
    int     width = 0;
    bool    rightAligned = false;
};

void emitStatus(QIODevice *sink, const QString &message)
{
    streamToOr(sink, stderr,
               QStringLiteral("paths: %1\n").arg(message));
}

QString modeName(Backend::TransportationTypes::TransportationMode mode)
{
    return Backend::transportationModeToString(mode);
}

QString formatNumber(double value, int decimals)
{
    return QLocale(QLocale::English, QLocale::UnitedStates)
        .toString(value, 'f', decimals);
}

QString fitCell(const QString &text, int width, bool rightAligned = false)
{
    QString value = text;
    if (value.size() > width)
    {
        if (width <= 3)
            value = value.left(width);
        else
            value = value.left(width - 3) + QStringLiteral("...");
    }

    return rightAligned ? value.rightJustified(width)
                        : value.leftJustified(width);
}

QString renderTableRow(const QStringList &cells,
                       const QList<TableColumn> &columns)
{
    QStringList padded;
    padded.reserve(columns.size());
    for (int i = 0; i < columns.size(); ++i)
    {
        padded.append(fitCell(cells.value(i), columns.at(i).width,
                              columns.at(i).rightAligned));
    }
    return padded.join(QStringLiteral("  "));
}

QString renderTableHeader(const QList<TableColumn> &columns)
{
    QStringList headers;
    QStringList rules;
    headers.reserve(columns.size());
    rules.reserve(columns.size());

    for (const auto &column : columns)
    {
        headers.append(
            fitCell(column.header, column.width, column.rightAligned));
        rules.append(QString(column.width, QLatin1Char('-')));
    }

    return headers.join(QStringLiteral("  "))
           + QLatin1Char('\n')
           + rules.join(QStringLiteral("  "));
}

QString shortenedTerminalSuffix(const QString &terminalId)
{
    const int dash = terminalId.lastIndexOf(QLatin1Char('-'));
    if (dash >= 0 && dash + 1 < terminalId.size())
        return terminalId.mid(dash + 1);
    return terminalId.size() > 12 ? terminalId.right(12) : terminalId;
}

QString routeTerminalLabel(const QString &displayName,
                           const QString &terminalId,
                           bool disambiguate)
{
    if (displayName.isEmpty())
        return terminalId;
    if (!disambiguate)
        return displayName;
    return QStringLiteral("%1 [%2]")
        .arg(displayName, shortenedTerminalSuffix(terminalId));
}

QString routeText(
    const Backend::Application::PathPresentationRecord &record,
    const Backend::Application::PathPresentationSummary &summary)
{
    const QString originId =
        record.path ? record.path->getOriginId() : QString();
    const QString destinationId =
        record.path ? record.path->getDestinationId() : QString();

    const bool ambiguous =
        !summary.startTerminalName.isEmpty()
        && summary.startTerminalName == summary.endTerminalName;

    if (ambiguous)
    {
        return QStringLiteral("%1 [%2 -> %3]")
            .arg(summary.startTerminalName,
                 shortenedTerminalSuffix(originId),
                 shortenedTerminalSuffix(destinationId));
    }

    return QStringLiteral("%1 -> %2")
        .arg(routeTerminalLabel(summary.startTerminalName, originId,
                                ambiguous),
             routeTerminalLabel(summary.endTerminalName, destinationId,
                                ambiguous));
}

QString vehicleBreakdownText(
    const QList<Backend::Scenario::PathMetrics::VehicleRequirement>
        &requirements)
{
    QStringList parts;
    parts.reserve(requirements.size());
    for (const auto &requirement : requirements)
    {
        parts.append(QStringLiteral(
                         "{segment_index=%1,mode=%2,vehicles_needed=%3}")
                         .arg(requirement.segmentIndex)
                         .arg(modeName(requirement.mode))
                         .arg(requirement.vehiclesNeeded));
    }
    return QStringLiteral("[%1]")
        .arg(parts.join(QStringLiteral(",")));
}

QJsonArray vehicleBreakdownJson(
    const QList<Backend::Scenario::PathMetrics::VehicleRequirement>
        &requirements)
{
    QJsonArray array;
    for (const auto &requirement : requirements)
    {
        QJsonObject object;
        object[QStringLiteral("segment_index")] =
            requirement.segmentIndex;
        object[QStringLiteral("mode")] =
            modeName(requirement.mode);
        object[QStringLiteral("vehicles_needed")] =
            requirement.vehiclesNeeded;
        array.append(object);
    }
    return array;
}

QJsonObject eligibilityToJson(
    const Backend::Scenario::PreparedPathEligibility
        &eligibility)
{
    QJsonObject object;
    object[QStringLiteral("selectable")] =
        eligibility.selectable;
    object[QStringLiteral("simulatable")] =
        eligibility.simulatable;
    object[QStringLiteral("blocking_reason")] =
        eligibility.blockingReason;
    object[QStringLiteral("warning_reason")] =
        eligibility.warningReason;
    return object;
}

QJsonObject metricsToJson(
    const Backend::Scenario::PathMetrics &metrics)
{
    QJsonObject object;
    object[QStringLiteral("valid")] = metrics.valid;
    object[QStringLiteral("distance_km")] = metrics.distanceKm;
    object[QStringLiteral("travel_time_hours")] =
        metrics.travelTimeHours;
    object[QStringLiteral("risk_per_vehicle")] =
        metrics.riskPerVehicle;
    object[QStringLiteral("fuel_per_vehicle")] =
        metrics.fuelPerVehicle;
    object[QStringLiteral("energy_per_vehicle")] =
        metrics.energyPerVehicle;
    object[QStringLiteral("carbon_per_vehicle")] =
        metrics.carbonPerVehicle;
    object[QStringLiteral("fuel_type")] = metrics.fuelType;
    object[QStringLiteral("preview_container_count")] =
        metrics.containerCount;
    object[QStringLiteral("fuel_per_container")] =
        metrics.fuelPerContainer;
    object[QStringLiteral("energy_per_container")] =
        metrics.energyPerContainer;
    object[QStringLiteral("carbon_per_container")] =
        metrics.carbonPerContainer;
    object[QStringLiteral("risk_per_container")] =
        metrics.riskPerContainer;
    object[QStringLiteral("preview_vehicles_needed")] =
        metrics.vehiclesNeeded;
    object[QStringLiteral("preview_vehicle_breakdown")] =
        vehicleBreakdownJson(metrics.previewVehicleBreakdown);
    return object;
}

QJsonObject costSnapshotToJson(
    const Backend::Application::PathPresentationCostSnapshot
        &snapshot)
{
    QJsonObject object;
    object[QStringLiteral("available")] = snapshot.available;
    object[QStringLiteral("travel_time_cost")] =
        snapshot.travelTime;
    object[QStringLiteral("distance_cost")] =
        snapshot.distance;
    object[QStringLiteral("carbon_cost")] =
        snapshot.carbonEmissions;
    object[QStringLiteral("energy_cost")] =
        snapshot.energyConsumption;
    object[QStringLiteral("risk_cost")] = snapshot.risk;
    object[QStringLiteral("direct_cost")] =
        snapshot.directCost;
    return object;
}

QJsonObject terminalEntryToJson(
    const Backend::Application::PathPresentationTerminalEntry
        &entry)
{
    QJsonObject object;
    object[QStringLiteral("index")] = entry.index;
    object[QStringLiteral("id")] = entry.id;
    object[QStringLiteral("display_name")] =
        entry.displayName;
    object[QStringLiteral("canonical_name")] =
        entry.canonicalName;
    object[QStringLiteral("predicted_costs_skipped")] =
        entry.predictedCostsSkipped;
    object[QStringLiteral("predicted_skip_reason")] =
        entry.predictedSkipReason;

    const auto &values = entry.displayValues;
    object[QStringLiteral("predicted_available")] =
        values.predictedAvailable;
    object[QStringLiteral("predicted_handling_seconds")] =
        values.predictedHandlingSeconds;
    object[QStringLiteral("predicted_direct_cost_usd")] =
        values.predictedDirectCostUsd;
    object[QStringLiteral(
        "predicted_weighted_delay_contribution")] =
        values.predictedWeightedDelayContribution;
    object[QStringLiteral(
        "predicted_weighted_cost_contribution")] =
        values.predictedWeightedCostContribution;
    object[QStringLiteral(
        "predicted_weighted_total_contribution")] =
        values.predictedWeightedTotalContribution;
    return object;
}

QJsonObject segmentEntryToJson(
    const Backend::Application::PathPresentationSegmentEntry
        &entry)
{
    QJsonObject object;
    object[QStringLiteral("index")] = entry.index;
    object[QStringLiteral("from")] = entry.startTerminalId;
    object[QStringLiteral("to")] = entry.endTerminalId;
    object[QStringLiteral("from_name")] =
        entry.startTerminalName;
    object[QStringLiteral("to_name")] =
        entry.endTerminalName;
    object[QStringLiteral("mode")] = entry.modeName;
    object[QStringLiteral("description")] =
        entry.description;
    object[QStringLiteral("additional_attributes")] =
        entry.additionalAttributes;

    const auto &values = entry.displayValues;
    object[QStringLiteral("predicted_available")] =
        values.predictedAvailable;
    object[QStringLiteral("distance_km")] =
        values.predictedDistanceKm;
    object[QStringLiteral("travel_time_hours")] =
        values.predictedTravelTimeHours;
    object[QStringLiteral("carbon_per_vehicle")] =
        values.predictedCarbonEmissions;
    object[QStringLiteral("energy_per_vehicle")] =
        values.predictedEnergyConsumption;
    object[QStringLiteral("risk_per_vehicle")] =
        values.predictedRisk;
    object[QStringLiteral("predicted_costs")] =
        costSnapshotToJson(entry.predictedCosts);
    return object;
}

QJsonObject pathRecordToJson(
    const Backend::Application::PathPresentationRecord &record,
    const Backend::Application::PathPresentationSummary &summary,
    const QList<Backend::Application::PathPresentationTerminalEntry>
        &terminals,
    const QList<Backend::Application::PathPresentationSegmentEntry>
        &segments)
{
    QJsonObject object;
    object[QStringLiteral("path_identity")] =
        record.pathIdentity;
    object[QStringLiteral("canonical_path_key")] =
        record.path ? record.path->canonicalPathKey()
                    : QString();
    object[QStringLiteral("path_id")] =
        record.path ? record.path->getPathId() : -1;
    object[QStringLiteral("path_uid")] =
        record.path ? record.path->getPathUid() : QString();
    object[QStringLiteral("rank")] =
        record.path ? record.path->getRank() : -1;
    object[QStringLiteral("origin")] =
        record.path ? record.path->getOriginId() : QString();
    object[QStringLiteral("destination")] =
        record.path ? record.path->getDestinationId()
                    : QString();
    object[QStringLiteral("path_label")] =
        summary.pathLabel;
    object[QStringLiteral("segment_count")] =
        summary.segmentCount;
    object[QStringLiteral("terminal_count")] =
        summary.terminalCount;
    object[QStringLiteral("predicted_total_cost")] =
        summary.predictedTotalCost;
    object[QStringLiteral("predicted_edge_cost")] =
        summary.predictedEdgeCost;
    object[QStringLiteral("predicted_terminal_cost")] =
        summary.predictedTerminalCost;
    object[QStringLiteral("eligibility")] =
        eligibilityToJson(record.eligibility);
    object[QStringLiteral("predicted_metrics")] =
        metricsToJson(record.predictedMetrics);

    QJsonArray terminalsArray;
    for (const auto &terminal : terminals)
        terminalsArray.append(terminalEntryToJson(terminal));
    object[QStringLiteral("terminals")] = terminalsArray;

    QJsonArray segmentsArray;
    for (const auto &segment : segments)
        segmentsArray.append(segmentEntryToJson(segment));
    object[QStringLiteral("segments")] = segmentsArray;

    return object;
}

QString summaryLine(
    const Backend::Application::PathPresentationRecord &record,
    const Backend::Application::PathPresentationSummary &summary)
{
    const auto &metrics = record.predictedMetrics;

    static const QList<TableColumn> kColumns = {
        {QStringLiteral("Rank"), 4, true},
        {QStringLiteral("Route"), 42, false},
        {QStringLiteral("Total USD"), 12, true},
        {QStringLiteral("Edge USD"), 12, true},
        {QStringLiteral("Terminal USD"), 12, true},
        {QStringLiteral("Km"), 10, true},
        {QStringLiteral("Hours"), 8, true},
        {QStringLiteral("Energy kWh"), 13, true},
        {QStringLiteral("CO2 t"), 9, true},
        {QStringLiteral("Veh"), 5, true},
        {QStringLiteral("Seg"), 5, true},
        {QStringLiteral("Term"), 5, true},
    };

    return renderTableRow(
        {QString::number(record.path ? record.path->getRank() : -1),
         routeText(record, summary),
         formatNumber(summary.predictedTotalCost, 2),
         formatNumber(summary.predictedEdgeCost, 2),
         formatNumber(summary.predictedTerminalCost, 2),
         formatNumber(metrics.distanceKm, 1),
         formatNumber(metrics.travelTimeHours, 1),
         formatNumber(metrics.energyPerVehicle, 1),
         formatNumber(metrics.carbonPerVehicle, 2),
         QString::number(metrics.vehiclesNeeded),
         QString::number(summary.segmentCount),
         QString::number(summary.terminalCount)},
        kColumns);
}

QString terminalLine(
    const Backend::Application::PathPresentationTerminalEntry
        &entry)
{
    const auto &values = entry.displayValues;

    static const QList<TableColumn> kColumns = {
        {QStringLiteral("#"), 2, true},
        {QStringLiteral("Terminal"), 34, false},
        {QStringLiteral("Handling s"), 12, true},
        {QStringLiteral("Direct USD"), 12, true},
        {QStringLiteral("Delay Wt"), 10, true},
        {QStringLiteral("Cost Wt"), 10, true},
        {QStringLiteral("Total Wt"), 10, true},
    };

    return renderTableRow(
        {QString::number(entry.index), entry.displayName,
         formatNumber(values.predictedHandlingSeconds, 1),
         formatNumber(values.predictedDirectCostUsd, 2),
         formatNumber(values.predictedWeightedDelayContribution,
                      2),
         formatNumber(values.predictedWeightedCostContribution,
                      2),
         formatNumber(values.predictedWeightedTotalContribution,
                      2)},
        kColumns);
}

QString segmentLine(
    const Backend::Application::PathPresentationSegmentEntry
        &entry)
{
    const auto &values = entry.displayValues;

    static const QList<TableColumn> kColumns = {
        {QStringLiteral("#"), 2, true},
        {QStringLiteral("Leg"), 38, false},
        {QStringLiteral("Mode"), 6, false},
        {QStringLiteral("Km"), 10, true},
        {QStringLiteral("Hours"), 8, true},
        {QStringLiteral("Energy"), 12, true},
        {QStringLiteral("CO2 t"), 9, true},
        {QStringLiteral("Risk"), 8, true},
        {QStringLiteral("Direct USD"), 12, true},
    };

    return renderTableRow(
        {QString::number(entry.index), entry.description,
         entry.modeName, formatNumber(values.predictedDistanceKm, 1),
         formatNumber(values.predictedTravelTimeHours, 1),
         formatNumber(values.predictedEnergyConsumption, 1),
         formatNumber(values.predictedCarbonEmissions, 2),
         formatNumber(values.predictedRisk, 4),
         formatNumber(entry.predictedCosts.directCost, 2)},
        kColumns);
}

QString summaryHeader()
{
    static const QList<TableColumn> kColumns = {
        {QStringLiteral("Rank"), 4, true},
        {QStringLiteral("Route"), 42, false},
        {QStringLiteral("Total USD"), 12, true},
        {QStringLiteral("Edge USD"), 12, true},
        {QStringLiteral("Terminal USD"), 12, true},
        {QStringLiteral("Km"), 10, true},
        {QStringLiteral("Hours"), 8, true},
        {QStringLiteral("Energy kWh"), 13, true},
        {QStringLiteral("CO2 t"), 9, true},
        {QStringLiteral("Veh"), 5, true},
        {QStringLiteral("Seg"), 5, true},
        {QStringLiteral("Term"), 5, true},
    };

    return renderTableHeader(kColumns);
}

QString terminalHeader()
{
    static const QList<TableColumn> kColumns = {
        {QStringLiteral("#"), 2, true},
        {QStringLiteral("Terminal"), 34, false},
        {QStringLiteral("Handling s"), 12, true},
        {QStringLiteral("Direct USD"), 12, true},
        {QStringLiteral("Delay Wt"), 10, true},
        {QStringLiteral("Cost Wt"), 10, true},
        {QStringLiteral("Total Wt"), 10, true},
    };

    return renderTableHeader(kColumns);
}

QString segmentHeader()
{
    static const QList<TableColumn> kColumns = {
        {QStringLiteral("#"), 2, true},
        {QStringLiteral("Leg"), 38, false},
        {QStringLiteral("Mode"), 6, false},
        {QStringLiteral("Km"), 10, true},
        {QStringLiteral("Hours"), 8, true},
        {QStringLiteral("Energy"), 12, true},
        {QStringLiteral("CO2 t"), 9, true},
        {QStringLiteral("Risk"), 8, true},
        {QStringLiteral("Direct USD"), 12, true},
    };

    return renderTableHeader(kColumns);
}

bool parseArgs(const QStringList &args, Options &options,
               QString *error)
{
    QStringList positional;

    for (int i = 0; i < args.size(); ++i)
    {
        const QString &arg = args.at(i);
        if (arg == QLatin1String("--json"))
        {
            options.json = true;
            continue;
        }
        if (arg == QLatin1String("--details"))
        {
            options.details = true;
            continue;
        }
        if (arg == QLatin1String("--top"))
        {
            if (i + 1 >= args.size())
            {
                *error = QStringLiteral(
                    "paths: --top requires a positive integer\n");
                return false;
            }

            bool ok = false;
            const int value = args.at(++i).toInt(&ok);
            if (!ok || value <= 0)
            {
                *error = QStringLiteral(
                    "paths: --top requires a positive integer\n");
                return false;
            }

            options.hasTopOverride = true;
            options.topOverride = value;
            continue;
        }
        if (arg.startsWith(QLatin1Char('-')))
        {
            *error = QStringLiteral(
                "paths: unsupported flag '%1' "
                "(supported: --top N, --json, --details)\n")
                         .arg(arg);
            return false;
        }
        positional.append(arg);
    }

    if (options.json && options.details)
    {
        *error = QStringLiteral(
            "paths: --json and --details cannot be used together\n");
        return false;
    }

    if (positional.size() != 1)
    {
        *error = QStringLiteral(
            "paths: expected exactly one scenario argument\n");
        return false;
    }

    options.scenarioPath = positional.first();
    return true;
}

} // namespace

PathsCommand::PathsCommand(QIODevice *outSink, QIODevice *errSink)
    : m_out(outSink)
    , m_err(errSink)
{
}

int PathsCommand::execute(const QStringList &args)
{
    qCInfo(lcCli) << "PathsCommand::execute: entry, args =" << args;

    Options options;
    QString error;
    if (!parseArgs(args, options, &error))
    {
        qCWarning(lcCli)
            << "PathsCommand::execute: bad arguments -"
            << error.trimmed();
        streamToOr(m_err, stderr, error);
        return static_cast<int>(ExitCode::BadArgs);
    }

    Backend::Application::ScenarioLoadService loadService;
    auto parseResult =
        loadService.parseAndValidateYaml(options.scenarioPath);
    if (parseResult.status
            == Backend::Application::ScenarioLoadServiceStatus::
                   ParseFailed
        || parseResult.status
               == Backend::Application::ScenarioLoadServiceStatus::
                      InvalidInput)
    {
        const QString suffix = parseResult.message.isEmpty()
            ? QString()
            : QStringLiteral(": ") + parseResult.message;
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "paths: failed to parse %1%2\n")
                       .arg(options.scenarioPath, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    bool hasError = false;
    const QString issueBuffer =
        formatValidationIssues(parseResult.issues, &hasError);
    if (!issueBuffer.isEmpty())
        streamToOr(m_err, stderr, issueBuffer);
    if (!parseResult.succeeded())
        return static_cast<int>(ExitCode::ValidationFailed);

    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::BackendBootstrapService bootstrapService;
    const auto bootstrapResult =
        bootstrapService.initializeAndStartController(QString());
    if (!bootstrapResult.succeeded())
    {
        const QString reason = bootstrapResult.message.isEmpty()
            ? QStringLiteral("backend bootstrap failed")
            : bootstrapResult.message;
        streamToOr(m_err, stderr,
                   QStringLiteral("paths: %1\n").arg(reason));
        return static_cast<int>(ExitCode::ConnectTimeout);
    }
    auto stopGuard = qScopeGuard([&controller]() {
        controller.stopAll();
    });
    emitStatus(m_err, QStringLiteral("backend clients started"));

    auto loadResult =
        loadService.loadValidatedDocument(
            std::move(parseResult.document));
    if (!loadResult.succeeded())
    {
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "paths: scenario apply failed: %1\n")
                       .arg(loadResult.message));
        return static_cast<int>(ExitCode::RunFailed);
    }
    Backend::Scenario::ScenarioRuntime &runtime =
        *loadResult.runtime;
    emitStatus(m_err, QStringLiteral("scenario loaded"));

    const int topN = options.hasTopOverride
        ? options.topOverride
        : controller.getSimulationParams()
              .value(QStringLiteral("shortest_paths"), 5)
              .toInt();
    emitStatus(m_err,
               QStringLiteral(
                   "discovering candidate paths (topN=%1)")
                   .arg(topN));

    Backend::Application::PreparedPathService
        preparedPathService(&controller);
    const auto preparedResult =
        preparedPathService.discoverAndPrepare(runtime, topN);
    if (!preparedResult.succeeded())
    {
        const QString reason = preparedResult.message.isEmpty()
            ? QStringLiteral("prepared-path workflow failed")
            : preparedResult.message;
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "paths: path discovery failed: %1\n")
                       .arg(reason));
        return static_cast<int>(
            preparedResult.status
                    == Backend::Application::
                           PreparedPathServiceStatus::
                               BackendUnavailable
                ? ExitCode::ConnectTimeout
                : ExitCode::RunFailed);
    }

    emitStatus(m_err,
               QStringLiteral("discovered %1 path(s)")
                   .arg(preparedResult.preparedPathCount));

    Backend::Application::PathPresentationService
        presentationService;
    const auto records =
        presentationService.recordsFromPreparedPaths(
            preparedResult.preparedPaths);

    if (options.json)
    {
        QJsonObject root;
        root[QStringLiteral("top_n_requested")] =
            preparedResult.topNRequested;
        root[QStringLiteral("discovered_path_count")] =
            preparedResult.preparedPathCount;

        QJsonArray paths;
        for (const auto &record : records)
        {
            const auto summary =
                presentationService.summary(record);
            const auto terminals =
                presentationService.terminalEntries(record);
            const auto segments =
                presentationService.segmentEntries(record);
            paths.append(pathRecordToJson(record, summary,
                                          terminals, segments));
        }
        root[QStringLiteral("paths")] = paths;

        streamToOr(
            m_out, stdout,
            QJsonDocument(root).toJson(QJsonDocument::Indented));
        return static_cast<int>(ExitCode::Success);
    }

    QStringList lines;
    lines.reserve(records.size() * (options.details ? 8 : 1) + 8);
    lines.append(QStringLiteral("Discovered %1 path(s) (requested top %2)")
                     .arg(preparedResult.preparedPathCount)
                     .arg(preparedResult.topNRequested));
    lines.append(QString());
    lines.append(summaryHeader());

    for (const auto &record : records)
    {
        const auto summary =
            presentationService.summary(record);
        lines.append(summaryLine(record, summary));

        if (!options.details)
            continue;

        const auto &metrics = record.predictedMetrics;
        lines.append(QString());
        lines.append(QStringLiteral("Path %1").arg(
            record.path ? record.path->getRank() : -1));
        lines.append(QStringLiteral("  Route: %1")
                         .arg(routeText(record, summary)));
        lines.append(QStringLiteral(
                         "  Costs: total=%1 USD, edge=%2 USD, terminal=%3 USD")
                         .arg(formatNumber(summary.predictedTotalCost, 2),
                              formatNumber(summary.predictedEdgeCost, 2),
                              formatNumber(summary.predictedTerminalCost, 2)));
        lines.append(QStringLiteral(
                         "  Metrics: distance=%1 km, time=%2 h, energy=%3 kWh, carbon=%4 t, vehicles=%5")
                         .arg(formatNumber(metrics.distanceKm, 1),
                              formatNumber(metrics.travelTimeHours, 1),
                              formatNumber(metrics.energyPerVehicle, 1),
                              formatNumber(metrics.carbonPerVehicle, 2),
                              QString::number(metrics.vehiclesNeeded)));
        lines.append(QStringLiteral("  Vehicle breakdown: %1")
                         .arg(vehicleBreakdownText(
                             metrics.previewVehicleBreakdown)));

        const auto terminals =
            presentationService.terminalEntries(record);
        lines.append(QStringLiteral("  Terminals"));
        lines.append(QStringLiteral("  %1")
                         .arg(terminalHeader()));
        for (const auto &terminal : terminals)
            lines.append(QStringLiteral("  %1")
                             .arg(terminalLine(terminal)));

        const auto segments =
            presentationService.segmentEntries(record);
        lines.append(QStringLiteral("  Segments"));
        lines.append(QStringLiteral("  %1")
                         .arg(segmentHeader()));
        for (const auto &segment : segments)
            lines.append(QStringLiteral("  %1")
                             .arg(segmentLine(segment)));
    }

    if (!options.details)
    {
        lines.append(QString());
        lines.append(QStringLiteral(
            "Use --details for terminal and segment breakdowns, or --json for machine-readable output."));
    }

    lines.append(QString());
    streamToOr(m_out, stdout,
               lines.join(QLatin1Char('\n')).toUtf8());
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
