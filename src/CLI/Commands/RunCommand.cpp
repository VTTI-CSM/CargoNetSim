#include "RunCommand.h"

#include <QDir>
#include <QEventLoop>
#include <QIODevice>
#include <QScopeGuard>

#include <cstdio>
#include <functional>

#include "Backend/Application/PreparedPathService.h"
#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/Application/SimulationRunService.h"
#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/CliApi/ResultsApi.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
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

/// Argument shape for `run`. The parity target is explicit selection;
/// Track A1 currently exposes only `--all`, while a bare scenario path
/// remains a documented temporary alias for `--all`.
struct Options
{
    QString scenarioPath;
    bool    selectAll = false;
};

bool parseArgs(const QStringList &args, Options &o, QString *err)
{
    QStringList positional;

    for (const QString &arg : args)
    {
        if (arg == QLatin1String("--all"))
        {
            o.selectAll = true;
            continue;
        }
        if (arg.startsWith(QLatin1Char('-')))
        {
            *err = QStringLiteral(
                "run: unsupported flag '%1' "
                "(supported today: --all)\n")
                .arg(arg);
            return false;
        }
        positional.append(arg);
    }

    if (positional.size() != 1)
    {
        *err = QStringLiteral(
            "run: expected exactly one scenario argument "
            "and optional --all\n");
        return false;
    }

    o.scenarioPath = positional.first();
    if (!o.selectAll)
    {
        // Transition behavior: bare `run scenario.yml` still maps to
        // `--all` until narrower subset-addressing is introduced.
        o.selectAll = true;
    }
    return true;
}

void emitStatus(QIODevice *sink, const QString &message)
{
    streamToOr(sink, stderr,
               QStringLiteral("run: %1\n").arg(message));
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
    emitStatus(m_err, QStringLiteral("scenario loaded"));

    // ---- 5. Path discovery ---------------------------------------------
    const int n = ctl.getSimulationParams()
                      .value("shortest_paths", 5).toInt();
    qCDebug(lcCli) << "RunCommand::execute: [stage 5] running path discovery (shortestPathsN ="
                   << n << ")...";
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
    if (opt.selectAll)
    {
        const auto selectionResult =
            runService.selectAndValidate(
                rt, prepared.pathIdentities());
        if (!selectionResult.succeeded())
        {
            streamToOr(m_err, stderr,
                       QStringLiteral("run: %1\n")
                           .arg(selectionResult.message.isEmpty()
                                    ? QStringLiteral(
                                          "failed to select prepared paths")
                                    : selectionResult.message));
            return static_cast<int>(
                selectionResult.status
                    == Backend::Application::SimulationRunServiceStatus::ValidationFailed
                ? ExitCode::ConnectTimeout
                : ExitCode::RunFailed);
        }
    }

    const QList<Backend::Path *> simulationSet = rt.paths();

    if (simulationSet.isEmpty())
    {
        streamToOr(m_err, stderr,
                   QStringLiteral("run: no paths selected for simulation\n"));
        return static_cast<int>(ExitCode::RunFailed);
    }

    emitStatus(m_err,
               QStringLiteral("selected %1 path(s) for simulation")
                   .arg(simulationSet.size()));

    // ---- 7. Progress reporter + status streaming -----------------------
    // Routes through the same sink as our diagnostics so tests (and
    // users who redirect stderr) capture everything in one stream.
    qCDebug(lcCli) << "RunCommand::execute: [stage 7] attaching progress reporter...";
    ProgressReporter reporter(/*quiet=*/false, m_err);
    QObject::connect(&rt, &ScenarioRuntime::progressChanged,
                     [&reporter](double t, double p) {
                         reporter.report(t, p);
                     });
    QObject::connect(&rt, &ScenarioRuntime::statusMessage,
                     [this](const QString &message) {
                         emitStatus(m_err, message);
                     });

    // ---- 8. Start + block until completed / failed ---------------------
    qCInfo(lcCli) << "RunCommand::execute: [stage 8] starting simulation...";
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
                startResult.status
                    == Backend::Application::SimulationRunServiceStatus::ValidationFailed
                ? ExitCode::ConnectTimeout
                : ExitCode::RunFailed);
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

    qCInfo(lcCli) << "RunCommand::execute: finished — exit code = Success";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
