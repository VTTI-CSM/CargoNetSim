#include "RunCommand.h"

#include <QDir>
#include <QEventLoop>
#include <QIODevice>
#include <QScopeGuard>
#include <QSet>

#include <cstdio>
#include <functional>

#include "Backend/Application/PreparedPathService.h"
#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/Application/SimulationRunService.h"
#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/CliApi/ResultsApi.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"
#include "CLI/Output/CsvResultsWriter.h"
#include "CLI/Output/JsonResultsWriter.h"
#include "CLI/Progress/ProgressReporter.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// Argument shape for `run`. `--all` selects every prepared candidate
/// path; `--paths` selects a comma-separated subset by the 1-based
/// presentation index shown by the `discover` command. Selection is
/// translated to stable prepared-path identities after discovery.
/// A bare scenario path remains a documented alias for `--all`.
struct Options
{
    QString      scenarioPath;
    bool         selectAll = false;
    bool         verbose = false;
    bool         hasTopOverride = false;
    int          topOverride = 0;
    QVector<int> selectedPathIndexes;
};

bool parsePositiveIntOption(const QString &optionName,
                            const QString &value,
                            int           *parsed,
                            QString       *err)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    if (!ok || result <= 0)
    {
        *err = QStringLiteral(
                   "run: %1 requires a positive integer\n")
                   .arg(optionName);
        return false;
    }

    *parsed = result;
    return true;
}

bool parsePathSelection(const QString &value,
                        QVector<int>  &indexes,
                        QString       *err)
{
    if (value.trimmed().isEmpty())
    {
        *err = QStringLiteral(
            "run: --paths requires a comma-separated list of positive path indexes\n");
        return false;
    }

    QVector<int> parsed;
    QSet<int>    seen;
    const QStringList parts =
        value.split(QLatin1Char(','), Qt::KeepEmptyParts);
    for (const QString &part : parts)
    {
        const QString token = part.trimmed();
        bool          ok = false;
        const int     index = token.toInt(&ok);
        if (!ok || index <= 0)
        {
            *err = QStringLiteral(
                       "run: --paths requires positive integer path indexes; got '%1'\n")
                       .arg(token);
            return false;
        }
        if (seen.contains(index))
        {
            *err = QStringLiteral(
                       "run: --paths contains duplicate path index '%1'\n")
                       .arg(index);
            return false;
        }

        seen.insert(index);
        parsed.append(index);
    }

    indexes = std::move(parsed);
    return true;
}

bool parseArgs(const QStringList &args, Options &o, QString *err)
{
    QStringList positional;

    for (int i = 0; i < args.size(); ++i)
    {
        const QString &arg = args.at(i);
        if (arg == QLatin1String("--all"))
        {
            o.selectAll = true;
            continue;
        }
        if (arg == QLatin1String("--paths"))
        {
            if (i + 1 >= args.size())
            {
                *err = QStringLiteral(
                    "run: --paths requires a comma-separated list of positive path indexes\n");
                return false;
            }
            if (!parsePathSelection(args.at(++i),
                                    o.selectedPathIndexes, err))
                return false;
            continue;
        }
        if (arg.startsWith(QLatin1String("--paths=")))
        {
            const QString value =
                arg.mid(QStringLiteral("--paths=").size());
            if (!parsePathSelection(value,
                                    o.selectedPathIndexes, err))
                return false;
            continue;
        }
        if (arg == QLatin1String("--verbose"))
        {
            o.verbose = true;
            continue;
        }
        if (arg == QLatin1String("--top"))
        {
            if (i + 1 >= args.size())
            {
                *err = QStringLiteral(
                    "run: --top requires a positive integer\n");
                return false;
            }
            if (!parsePositiveIntOption(
                    QStringLiteral("--top"),
                    args.at(++i), &o.topOverride, err))
                return false;
            o.hasTopOverride = true;
            continue;
        }
        if (arg.startsWith(QLatin1String("--top=")))
        {
            if (!parsePositiveIntOption(
                    QStringLiteral("--top"),
                    arg.mid(QStringLiteral("--top=").size()),
                    &o.topOverride, err))
                return false;
            o.hasTopOverride = true;
            continue;
        }
        if (arg.startsWith(QLatin1Char('-')))
        {
            *err = QStringLiteral(
                "run: unsupported flag '%1' "
                "(supported: --all, --paths LIST, --top N, --verbose)\n")
                .arg(arg);
            return false;
        }
        positional.append(arg);
    }

    if (o.selectAll && !o.selectedPathIndexes.isEmpty())
    {
        *err = QStringLiteral(
            "run: --all and --paths cannot be used together\n");
        return false;
    }

    if (positional.size() != 1)
    {
        *err = QStringLiteral(
            "run: expected exactly one scenario argument "
            "and optional --all/--paths/--top/--verbose\n");
        return false;
    }

    o.scenarioPath = positional.first();
    if (!o.selectAll && o.selectedPathIndexes.isEmpty())
    {
        // Transition behavior: bare `run scenario.yml` still maps to
        // `--all` for existing scripts.
        o.selectAll = true;
    }
    return true;
}

ExitCode exitCodeForRunServiceFailure(
    Backend::Application::SimulationRunServiceStatus status)
{
    using Status =
        Backend::Application::SimulationRunServiceStatus;

    switch (status)
    {
    case Status::InvalidSelection:
        return ExitCode::BadArgs;
    case Status::ValidationFailed:
        // SimulationRunService validation includes simulator command
        // availability checks for the selected path modes. Surface that
        // as the public "required simulator unavailable" exit contract.
        return ExitCode::ConnectTimeout;
    case Status::StartFailed:
        return ExitCode::RunFailed;
    case Status::Success:
        return ExitCode::Success;
    }

    return ExitCode::RunFailed;
}

bool selectedPathIdentitiesForIndexes(
    const Backend::Scenario::PreparedPathSet &prepared,
    const QVector<int>                       &indexes,
    QVector<QString>                         *selected,
    QString                                  *err)
{
    selected->clear();
    selected->reserve(indexes.size());

    const auto &records = prepared.records();
    for (int presentationIndex : indexes)
    {
        const int zeroBasedIndex = presentationIndex - 1;
        if (zeroBasedIndex < 0
            || zeroBasedIndex >= static_cast<int>(records.size()))
        {
            *err = QStringLiteral(
                       "run: --paths requested path index %1, but only %2 path(s) were discovered\n")
                       .arg(presentationIndex)
                       .arg(records.size());
            return false;
        }

        const QString &identity =
            records.at(static_cast<size_t>(zeroBasedIndex)).pathIdentity;
        if (identity.isEmpty())
        {
            *err = QStringLiteral(
                       "run: discovered path index %1 does not have a stable path identity\n")
                       .arg(presentationIndex);
            return false;
        }
        selected->append(identity);
    }

    return true;
}

void emitStatus(QIODevice *sink, const QString &message)
{
    streamToOr(sink, stderr,
               QStringLiteral("run: %1\n").arg(message));
}

const Backend::Path *findPathForResult(
    const QList<Backend::Path *> &paths,
    const Backend::Scenario::PathSimulationResult &result)
{
    for (const auto *path : paths)
    {
        if (!path)
            continue;
        if (!result.canonicalPathKey.isEmpty()
            && path->canonicalPathKey() == result.canonicalPathKey)
            return path;
        if (!result.pathUid.isEmpty()
            && path->getPathUid() == result.pathUid)
            return path;
        if (path->getPathId() == result.pathId
            && path->getRank() == result.rank)
            return path;
    }
    return nullptr;
}

QString modeSequenceForPath(const Backend::Path *path)
{
    if (!path)
        return QStringLiteral("unknown");

    QStringList modes;
    for (const auto *segment : path->getSegments())
    {
        if (!segment)
            continue;
        modes.append(Backend::TransportationTypes::toString(
            segment->getMode()));
    }

    return modes.isEmpty()
        ? QStringLiteral("unknown")
        : modes.join(QStringLiteral(" -> "));
}

int segmentCountForPath(const Backend::Path *path)
{
    return path ? path->getSegments().size() : 0;
}

QString money(double value)
{
    return QStringLiteral("$%1").arg(value, 0, 'f', 2);
}

void emitResultsSummary(
    QIODevice *sink,
    const Backend::Scenario::ScenarioDocument &doc,
    const QList<Backend::Scenario::PathSimulationResult> &results,
    const QList<Backend::Path *> &paths)
{
    const QString outDir = doc.output.directory.isEmpty()
        ? QDir::currentPath()
        : doc.output.directory;
    const QDir outputDir(outDir);

    streamToOr(sink, stderr,
               QStringLiteral(
                   "run: results saved to %1\n")
                   .arg(outputDir.absolutePath()));
    for (const auto &fmt : doc.output.formats)
    {
        if (fmt == QLatin1String("json")
            || fmt == QLatin1String("csv"))
        {
            streamToOr(sink, stderr,
                       QStringLiteral("run:   %1: %2\n")
                           .arg(fmt,
                                outputDir.absoluteFilePath(
                                    QStringLiteral("results.%1")
                                        .arg(fmt))));
        }
    }

    streamToOr(sink, stderr,
               QStringLiteral(
                   "run: summary: %1 path(s) simulated\n")
                   .arg(results.size()));

    for (const auto &result : results)
    {
        const Backend::Path *path =
            findPathForResult(paths, result);
        const int segmentCount = segmentCountForPath(path);
        const QString modes = modeSequenceForPath(path);

        streamToOr(
            sink, stderr,
            QStringLiteral(
                "run:   path_id=%1 rank=%2 containers=%3 segments=%4 [%5] total=%6 edges=%7 terminals=%8\n")
                .arg(result.pathId)
                .arg(result.rank)
                .arg(result.effectiveContainerCount)
                .arg(segmentCount)
                .arg(modes)
                .arg(money(result.totalCost),
                     money(result.edgeCosts),
                     money(result.terminalCosts)));
    }
}

bool waitForSimulationEnd(Backend::Scenario::ScenarioRuntime &rt,
                          QString                            *failMsg,
                          const std::function<bool()>       &starter)
{
    QEventLoop loop;
    bool       completed = false;
    bool       failed    = false;

    QObject::connect(&rt, &Backend::Scenario::ScenarioRuntime::completed,
                     &loop, [&] {
                         completed = true;
                         loop.quit();
                     });
    QObject::connect(&rt, &Backend::Scenario::ScenarioRuntime::failed,
                     &loop, [&](const QString &m) {
                         failed = true;
                         if (failMsg) *failMsg = m;
                         loop.quit();
                     });

    if (!starter())
        return false;

    if (!completed && !failed)
        loop.exec();
    return completed && !failed;
}

} // namespace

RunCommand::RunCommand(QIODevice *errSink)
    : m_err(errSink)
{
}

RunCommand::WriterHooks RunCommand::defaultWriterHooks()
{
    return WriterHooks{
        [](const QString &path) { return QDir().mkpath(path); },
        [](const QString &outputPath,
           const QList<Backend::Scenario::PathSimulationResult> &results,
           QString *err,
           const QHash<QString, Backend::Scenario::PathMetrics> &metrics,
           const QHash<QString, Backend::Scenario::PathKey> &keys,
           const QList<Backend::Path *> &paths) {
            JsonResultsWriter writer;
            return writer.write(outputPath, results, err, metrics,
                                keys, paths);
        },
        [](const QString &outputPath,
           const QList<Backend::Scenario::PathSimulationResult> &results,
           QString *err) {
            CsvResultsWriter writer;
            return writer.write(outputPath, results, err);
        }};
}

int RunCommand::writeOutputs(
    const Backend::Scenario::ScenarioDocument &doc,
    const QList<Backend::Scenario::PathSimulationResult> &results,
    const QList<Backend::Path *> &paths,
    const QHash<QString, Backend::Scenario::PathMetrics>
        &predictedMetricsByCanonicalPath,
    const QHash<QString, Backend::Scenario::PathKey>
        &pathKeysByCanonicalPath,
    const WriterHooks            &hooks) const
{
    const QString outDir = doc.output.directory.isEmpty()
        ? QDir::currentPath()
        : doc.output.directory;
    if (!hooks.mkpath || !hooks.mkpath(outDir))
    {
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "run: failed to create output directory '%1'\n")
                       .arg(outDir));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCDebug(lcCli) << "RunCommand::writeOutputs: output directory ="
                   << outDir;
    if (m_verbose)
        emitStatus(m_err,
                   QStringLiteral("writing outputs to %1").arg(outDir));

    bool writerFailed = false;
    for (const auto &fmt : doc.output.formats)
    {
        QString werr;
        bool    okWrite = true;
        if (fmt == QLatin1String("json"))
        {
            const QString filePath =
                QDir(outDir).filePath(QStringLiteral("results.json"));
            qCDebug(lcCli) << "RunCommand::writeOutputs: writing JSON results to"
                           << filePath;
            okWrite = hooks.writeJson
                && hooks.writeJson(filePath, results, &werr,
                                   predictedMetricsByCanonicalPath,
                                   pathKeysByCanonicalPath, paths);
        }
        else if (fmt == QLatin1String("csv"))
        {
            const QString filePath =
                QDir(outDir).filePath(QStringLiteral("results.csv"));
            qCDebug(lcCli) << "RunCommand::writeOutputs: writing CSV results to"
                           << filePath;
            okWrite = hooks.writeCsv
                && hooks.writeCsv(filePath, results, &werr);
        }
        else
        {
            qCWarning(lcCli)
                << "RunCommand::writeOutputs: unknown output format"
                << fmt;
            streamToOr(m_err, stderr,
                       QStringLiteral(
                           "run: ignoring unknown output format '%1'\n")
                           .arg(fmt));
            continue;
        }
        if (!okWrite)
        {
            writerFailed = true;
            qCCritical(lcCli) << "RunCommand::writeOutputs: writer"
                              << fmt << "failed —" << werr;
            streamToOr(m_err, stderr,
                       QStringLiteral("run: writer '%1': %2\n")
                           .arg(fmt, werr));
        }
        else
        {
            qCInfo(lcCli)
                << "RunCommand::writeOutputs: results written, format ="
                << fmt;
        }
    }

    if (writerFailed)
    {
        qCWarning(lcCli)
            << "RunCommand::writeOutputs: at least one requested output writer failed";
        return static_cast<int>(ExitCode::RunFailed);
    }

    return static_cast<int>(ExitCode::Success);
}

int RunCommand::execute(const QStringList &args)
{
    using namespace Backend::Scenario;

    qCInfo(lcCli) << "RunCommand::execute: entry, args =" << args;

    // ---- 1. Argument shape ---------------------------------------------
    Options opt;
    QString err;
    if (!parseArgs(args, opt, &err))
    {
        qCWarning(lcCli) << "RunCommand::execute: bad arguments —" << err.trimmed();
        streamToOr(m_err, stderr, err);
        return static_cast<int>(ExitCode::BadArgs);
    }
    m_verbose = opt.verbose;

    qCInfo(lcCli) << "RunCommand::execute: scenario path =" << opt.scenarioPath;

    // ---- 2. Parse + validate -------------------------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 2] parsing YAML...";
    Backend::Application::ScenarioLoadService loadService;
    auto parseResult =
        loadService.parseAndValidateYaml(opt.scenarioPath);
    if (parseResult.status == Backend::Application::ScenarioLoadServiceStatus::ParseFailed
        || parseResult.status == Backend::Application::ScenarioLoadServiceStatus::InvalidInput)
    {
        const QString suffix = parseResult.message.isEmpty()
            ? QString()
            : QStringLiteral(": ") + parseResult.message;
        qCCritical(lcCli) << "RunCommand::execute: YAML parse failed —"
                          << opt.scenarioPath << suffix;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: failed to parse %1%2\n")
                       .arg(opt.scenarioPath, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    bool hasError = false;
    const QString buffer =
        formatValidationIssues(parseResult.issues, &hasError);
    if (!buffer.isEmpty())
        streamToOr(m_err, stderr, buffer);
    if (!parseResult.succeeded())
    {
        qCCritical(lcCli) << "RunCommand::execute: validation failed with errors";
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    auto doc = std::move(parseResult.document);
    qCDebug(lcCli) << "RunCommand::execute: YAML parsed — regions ="
                   << doc->regions.size()
                   << ", terminals =" << doc->terminals.size()
                   << ", linkages =" << doc->linkages.size();

    // ---- 3. Bootstrap controller ---------------------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 3] bootstrapping controller...";
    auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
    Backend::BackendBootstrapService bootstrapService;
    const auto bootstrapResult =
        bootstrapService.initializeAndStartController(
            /*integrationExePath=*/QString());
    if (!bootstrapResult.succeeded())
    {
        const QString reason = bootstrapResult.message.isEmpty()
            ? QStringLiteral("backend bootstrap failed")
            : bootstrapResult.message;
        qCCritical(lcCli)
            << "RunCommand::execute: controller bootstrap failed —"
            << reason;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: %1\n").arg(reason));
        return static_cast<int>(ExitCode::ConnectTimeout);
    }

    // stopAll fires on every exit path from here on — success OR
    // failure. No leaked client threads.
    auto ctlGuard = qScopeGuard([&ctl] { ctl.stopAll(); });
    if (m_verbose)
        emitStatus(m_err, QStringLiteral("backend clients started"));

    // ---- 4. Construct runtime + apply scenario -------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 4] constructing runtime and applying scenario...";
    auto loadResult =
        loadService.loadValidatedDocument(std::move(doc));
    if (!loadResult.succeeded())
    {
        qCCritical(lcCli) << "RunCommand::execute: scenario apply (load) failed";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: scenario apply failed: %1\n")
                       .arg(loadResult.message));
        return static_cast<int>(ExitCode::RunFailed);
    }
    ScenarioRuntime &rt = *loadResult.runtime;
    qCDebug(lcCli) << "RunCommand::execute: scenario applied successfully";
    if (m_verbose)
        emitStatus(m_err, QStringLiteral("scenario loaded"));

    // ---- 5. Path discovery ---------------------------------------------
    const int n = opt.hasTopOverride
        ? opt.topOverride
        : ctl.getSimulationParams()
              .value("shortest_paths", 5).toInt();
    qCDebug(lcCli) << "RunCommand::execute: [stage 5] running path discovery (shortestPathsN ="
                   << n << ")...";
    if (m_verbose)
        emitStatus(m_err, QStringLiteral("discovering candidate paths"));
    Backend::Application::PreparedPathService preparedPathService(
        &ctl);
    auto preparedResult =
        preparedPathService.discoverAndPrepare(rt, n);
    if (!preparedResult.succeeded())
    {
        const QString reason = preparedResult.message.isEmpty()
            ? QStringLiteral("prepared-path workflow failed")
            : preparedResult.message;
        qCCritical(lcCli) << "RunCommand::execute: path discovery failed —" << reason;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: path discovery failed: %1\n")
                       .arg(reason));
        return static_cast<int>(
            preparedResult.status
                == Backend::Application::PreparedPathServiceStatus::BackendUnavailable
            ? ExitCode::ConnectTimeout
            : ExitCode::RunFailed);
    }
    qCDebug(lcCli) << "RunCommand::execute: path discovery found"
                   << preparedResult.preparedPathCount
                   << "paths";
    if (m_verbose)
        emitStatus(m_err,
                   QStringLiteral("discovered %1 path(s)")
                       .arg(preparedResult.preparedPathCount));

    auto prepared = std::move(preparedResult.preparedPaths);
    rt.setPreparedPaths(prepared);
    const auto predictedMetricsByCanonicalPath =
        prepared.predictedMetricsByCanonicalPath();
    const auto pathKeysByCanonicalPath =
        prepared.pathKeysByCanonicalPath();

    // ---- 6. Determine the selected simulation set ----------------------
    Backend::Application::SimulationRunService runService;
    QVector<QString> selectedPathIdentities;
    if (opt.selectAll)
    {
        selectedPathIdentities = prepared.pathIdentities();
    }
    else
    {
        QString selectionError;
        if (!selectedPathIdentitiesForIndexes(
                prepared, opt.selectedPathIndexes,
                &selectedPathIdentities, &selectionError))
        {
            streamToOr(m_err, stderr, selectionError);
            return static_cast<int>(ExitCode::BadArgs);
        }
    }

    const auto selectionResult =
        runService.selectAndValidate(
            rt, selectedPathIdentities,
            ExecutionDemandPolicy::DuplicateDemandPerSelectedPath);
    if (!selectionResult.succeeded())
    {
        streamToOr(m_err, stderr,
                   QStringLiteral("run: %1\n")
                       .arg(selectionResult.message.isEmpty()
                                ? QStringLiteral(
                                      "failed to select prepared paths")
                                : selectionResult.message));
        return static_cast<int>(
            exitCodeForRunServiceFailure(selectionResult.status));
    }

    const QList<Backend::Path *> simulationSet = rt.paths();

    if (simulationSet.isEmpty())
    {
        streamToOr(m_err, stderr,
                   QStringLiteral("run: no paths selected for simulation\n"));
        return static_cast<int>(ExitCode::RunFailed);
    }

    if (m_verbose)
    {
        emitStatus(m_err,
                   QStringLiteral(
                       "selected %1 path(s) for sequential duplicate-demand comparison")
                       .arg(simulationSet.size()));
    }

    // ---- 7. Progress reporter + status streaming -----------------------
    // Routes through the same sink as our diagnostics so tests (and
    // users who redirect stderr) capture everything in one stream.
    qCDebug(lcCli) << "RunCommand::execute: [stage 7] attaching progress reporter...";
    ProgressReporter reporter(
        /*quiet=*/false,
        m_err,
        m_verbose ? ProgressRenderMode::AppendLines
                  : ProgressRenderMode::Auto);
    reporter.setVerbose(m_verbose);
    QObject::connect(
        &rt, &ScenarioRuntime::progressSnapshotChanged,
        [&reporter](double t,
                    const ExecutionProgressSnapshot &snapshot) {
            reporter.reportSnapshot(t, snapshot);
        });
    if (m_verbose)
    {
        QObject::connect(&rt, &ScenarioRuntime::statusMessage,
                         [this](const QString &message) {
                             emitStatus(m_err, message);
                         });
    }

    // ---- 8. Start + block until completed / failed ---------------------
    qCInfo(lcCli) << "RunCommand::execute: [stage 8] starting simulation...";
    if (m_verbose)
        emitStatus(m_err, QStringLiteral("starting simulation"));
    QString       failMsg;
    Backend::Application::SimulationRunServiceResult startResult;
    const bool okRun = waitForSimulationEnd(
        rt, &failMsg,
        [&] {
            startResult = runService.validateAndStart(rt);
            if (!startResult.succeeded())
            {
                if (failMsg.isEmpty())
                    failMsg = startResult.message.isEmpty()
                        ? QStringLiteral("failed to start simulation")
                        : startResult.message;
                return false;
            }
            return true;
        });
    reporter.emitFinal(rt.progress());

    if (!okRun)
    {
        if (!startResult.succeeded())
        {
            streamToOr(m_err, stderr,
                       QStringLiteral("run: %1\n")
                           .arg(failMsg));
            return static_cast<int>(
                exitCodeForRunServiceFailure(startResult.status));
        }
        qCCritical(lcCli) << "RunCommand::execute: simulation failed —" << failMsg;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: %1\n").arg(failMsg));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCInfo(lcCli) << "RunCommand::execute: simulation completed successfully";

    // ---- 9. Write outputs ----------------------------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 9] writing results...";
    const auto results = rt.results();
    const int writeCode =
        writeOutputs(rt.document(), results, rt.paths(),
                     predictedMetricsByCanonicalPath,
                     pathKeysByCanonicalPath);
    if (writeCode != static_cast<int>(ExitCode::Success))
        return writeCode;

    emitResultsSummary(m_err, rt.document(), results, rt.paths());

    qCInfo(lcCli) << "RunCommand::execute: finished — exit code = Success";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
