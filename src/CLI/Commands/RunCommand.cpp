#include "RunCommand.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QIODevice>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTimer>

#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/ContainerAllocator.h"
#include "Backend/Scenario/PathAllocation.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/PathMetricsCalculator.h"
#include "Backend/Scenario/PathMetricsInputs.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/ScenarioValidator.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"
#include "CLI/Ipc/ControlChannel.h"
#include "CLI/Ipc/RuntimeStateFile.h"
#include "CLI/Output/CsvResultsWriter.h"
#include "CLI/Output/JsonResultsWriter.h"
#include "CLI/Progress/ProgressReporter.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// 30-second budget for every simulator client to emit
/// `allClientsReady`. Anything longer indicates RabbitMQ brokerage,
/// simulator executables, or the Qt-to-broker handshake is not
/// healthy and `run` should fail fast.
constexpr int kClientsReadyTimeoutMs = 30000;

/// Argument shape for `run`. Intentionally rigid: one scenario path,
/// no flags. Every simulation parameter comes from the YAML.
struct Options
{
    QString scenarioPath;
};

bool parseArgs(const QStringList &args, Options &o, QString *err)
{
    if (args.size() != 1)
    {
        *err = QStringLiteral(
            "run: expected exactly one argument (scenario.yml); "
            "no flags are accepted — every simulation parameter is "
            "read from the YAML itself\n");
        return false;
    }
    o.scenarioPath = args.first();
    return true;
}

/// Spin a local event loop until `CargoNetSimController::allClientsReady`
/// fires or @p timeoutMs elapses. Returns true iff the signal fired in
/// time. Local loop — does not commit `QCoreApplication::exec()`, so
/// `run` stays re-entrant even when called under a wrapper that might
/// have its own event loop (Task 13's `connections --watch`, tests).
bool waitForClientsReady(CargoNetSim::CargoNetSimController &ctl,
                         int                                 timeoutMs)
{
    QEventLoop waitLoop;
    QTimer     timeout;
    bool       ready = false;

    timeout.setSingleShot(true);
    QObject::connect(&ctl,
                     &CargoNetSim::CargoNetSimController::allClientsReady,
                     &waitLoop,
                     [&] { ready = true; waitLoop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout,
                     &waitLoop, &QEventLoop::quit);

    timeout.start(timeoutMs);
    waitLoop.exec();
    return ready;
}

/// Blocking wait on the `ScenarioRuntime` signals. Returns true for a
/// clean `completed`, false for `failed` (with message in @p failMsg).
bool waitForSimulationEnd(Backend::Scenario::ScenarioRuntime &rt,
                          QString                            *failMsg)
{
    QEventLoop loop;
    bool       failed = false;

    QObject::connect(&rt, &Backend::Scenario::ScenarioRuntime::completed,
                     &loop, &QEventLoop::quit);
    QObject::connect(&rt, &Backend::Scenario::ScenarioRuntime::failed,
                     &loop, [&](const QString &m) {
                         failed = true;
                         if (failMsg) *failMsg = m;
                         loop.quit();
                     });
    loop.exec();
    return !failed;
}

} // namespace

RunCommand::RunCommand(QIODevice *errSink)
    : m_err(errSink)
{
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
    QString parseErr;
    auto    doc = ScenarioSerializer::fromYaml(opt.scenarioPath,
                                               &parseErr);
    if (!doc)
    {
        const QString suffix = parseErr.isEmpty()
            ? QString()
            : QStringLiteral(": ") + parseErr;
        qCCritical(lcCli) << "RunCommand::execute: YAML parse failed —"
                          << opt.scenarioPath << suffix;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: failed to parse %1%2\n")
                       .arg(opt.scenarioPath, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    qCDebug(lcCli) << "RunCommand::execute: YAML parsed — regions ="
                   << doc->regions.size()
                   << ", terminals =" << doc->terminals.size()
                   << ", linkages =" << doc->linkages.size();

    {
        bool          hasError = false;
        const auto    issues   = ScenarioValidator::validate(*doc);
        qCDebug(lcCli) << "RunCommand::execute: [stage 2] validation complete — issues ="
                       << issues.size() << ", hasError =" << hasError;
        const QString buffer   =
            formatValidationIssues(issues, &hasError);
        if (!buffer.isEmpty())
            streamToOr(m_err, stderr, buffer);
        if (hasError)
        {
            qCCritical(lcCli) << "RunCommand::execute: validation failed with errors";
            return static_cast<int>(ExitCode::ValidationFailed);
        }
    }

    // ---- 3. Init controller + startAll + wait for ready ----------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 3] initializing controller...";
    auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
    if (!ctl.initialize(/*truckExePath=*/QString()))
    {
        qCCritical(lcCli) << "RunCommand::execute: controller init failed";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: controller init failed\n"));
        return static_cast<int>(ExitCode::ConnectTimeout);
    }

    // stopAll fires on every exit path from here on — success OR
    // failure. No leaked client threads.
    auto ctlGuard = qScopeGuard([&ctl] { ctl.stopAll(); });

    qCDebug(lcCli) << "RunCommand::execute: calling startAll...";
    if (!ctl.startAll())
    {
        qCCritical(lcCli) << "RunCommand::execute: startAll failed";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: startAll failed\n"));
        return static_cast<int>(ExitCode::ConnectTimeout);
    }

    qCInfo(lcCli) << "RunCommand::execute: waiting for clients ready (timeout ="
                  << kClientsReadyTimeoutMs << "ms)...";
    if (!waitForClientsReady(ctl, kClientsReadyTimeoutMs))
    {
        qCCritical(lcCli) << "RunCommand::execute: simulators not ready within"
                          << kClientsReadyTimeoutMs << "ms";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: simulators not ready within %1 ms\n")
                       .arg(kClientsReadyTimeoutMs));
        return static_cast<int>(ExitCode::ConnectTimeout);
    }
    qCInfo(lcCli) << "RunCommand::execute: all clients ready";

    // ---- 4. Construct runtime + apply scenario -------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 4] constructing runtime and applying scenario...";
    ScenarioRuntime rt(std::move(doc));
    if (!rt.load())
    {
        qCCritical(lcCli) << "RunCommand::execute: scenario apply (load) failed";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: scenario apply failed\n"));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCDebug(lcCli) << "RunCommand::execute: scenario applied successfully";

    // ---- 5. Path discovery ---------------------------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 5] running path discovery (shortestPathsN ="
                   << rt.document().simulation.shortestPathsN << ")...";
    PathDiscovery pd;
    QString       pdErr;
    const int     n     = rt.document().simulation.shortestPathsN;
    auto          paths =
        pd.findTopPaths(rt.document(), rt.registry(), n, &pdErr);
    if (paths.isEmpty())
    {
        const QString reason = pdErr.isEmpty()
            ? QStringLiteral("no origin/destination pairs in scenario")
            : pdErr;
        qCCritical(lcCli) << "RunCommand::execute: path discovery failed —" << reason;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: path discovery failed: %1\n")
                       .arg(reason));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCDebug(lcCli) << "RunCommand::execute: path discovery found" << paths.size() << "paths";
    rt.setPaths(paths);

    // ---- 6. State file (visible to sibling status/stop commands) -------
    qCDebug(lcCli) << "RunCommand::execute: [stage 6] writing runtime state file...";
    const qint64  pid        = QCoreApplication::applicationPid();
    const QString socketName =
        QStringLiteral("cargonetsim-cli-%1").arg(pid);
    {
        RuntimeStateEntry e;
        e.pid          = pid;
        e.socketName   = socketName;
        e.scenarioPath = opt.scenarioPath;
        e.startedAt    = QDateTime::currentDateTimeUtc();
        RuntimeStateFile::write(e, nullptr);
    }
    qCDebug(lcCli) << "RunCommand::execute: state file written, pid =" << pid
                   << ", socket =" << socketName;
    auto stateGuard = qScopeGuard(
        [pid] { RuntimeStateFile::removeForPid(pid); });

    // ---- 7. Control channel --------------------------------------------
    // ControlChannelServer's dtor closes the socket automatically, so
    // no explicit guard is needed — the local variable RAII handles
    // it on any return path.
    qCDebug(lcCli) << "RunCommand::execute: [stage 7] setting up IPC control channel...";
    ControlChannelServer ipc;
    ipc.setHandler([&rt](const QJsonObject &req) -> QJsonObject {
        const QString op = req.value(QStringLiteral("op")).toString();
        qCDebug(lcCli) << "RunCommand::ipcHandler: received op =" << op;
        if (op == QLatin1String("status"))
        {
            return QJsonObject{
                {QStringLiteral("state"),
                 rt.isRunning() ? QStringLiteral("running")
                                : QStringLiteral("idle")},
                {QStringLiteral("current_time"), rt.currentTime()},
                {QStringLiteral("end_time"),
                 rt.document().simulation.endTime},
                {QStringLiteral("progress"), rt.progress()}};
        }
        if (op == QLatin1String("stop"))
        {
            qCInfo(lcCli) << "RunCommand::ipcHandler: stop requested via IPC";
            rt.stop();
            return QJsonObject{{QStringLiteral("ok"), true}};
        }
        qCWarning(lcCli) << "RunCommand::ipcHandler: unknown op =" << op;
        return QJsonObject{
            {QStringLiteral("error"), QStringLiteral("unknown op")}};
    });
    ipc.listen(socketName);
    qCDebug(lcCli) << "RunCommand::execute: IPC channel listening on" << socketName;

    // ---- 8. Progress reporter ------------------------------------------
    // Routes through the same sink as our diagnostics so tests (and
    // users who redirect stderr) capture everything in one stream.
    qCDebug(lcCli) << "RunCommand::execute: [stage 8] attaching progress reporter...";
    ProgressReporter reporter(/*quiet=*/false, m_err);
    QObject::connect(&rt, &ScenarioRuntime::progressChanged,
                     [&reporter](double t, double p) {
                         reporter.report(t, p);
                     });

    // ---- 9. Start + block until completed / failed ---------------------
    qCInfo(lcCli) << "RunCommand::execute: [stage 9] starting simulation...";
    if (!rt.startSimulation())
    {
        qCCritical(lcCli) << "RunCommand::execute: startSimulation failed";
        streamToOr(m_err, stderr,
                   QStringLiteral("run: startSimulation failed\n"));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCInfo(lcCli) << "RunCommand::execute: simulation submitted, blocking until completion...";
    QString       failMsg;
    const bool    okRun = waitForSimulationEnd(rt, &failMsg);
    reporter.emitFinal(rt.progress());

    if (!okRun)
    {
        qCCritical(lcCli) << "RunCommand::execute: simulation failed —" << failMsg;
        streamToOr(m_err, stderr,
                   QStringLiteral("run: %1\n").arg(failMsg));
        return static_cast<int>(ExitCode::RunFailed);
    }
    qCInfo(lcCli) << "RunCommand::execute: simulation completed successfully";

    // ---- 10. Write outputs ---------------------------------------------
    qCDebug(lcCli) << "RunCommand::execute: [stage 10] writing results...";
    const QString outDir = rt.document().output.directory.isEmpty()
        ? QDir::currentPath()
        : rt.document().output.directory;
    QDir().mkpath(outDir);
    qCDebug(lcCli) << "RunCommand::execute: output directory =" << outDir;
    const auto results = rt.results();

    // Re-derive the allocator + per-path metrics here so `results.json`
    // carries the same per-vehicle AND per-container projections the
    // GUI shortest-paths table shows. Pure functions — no simulator
    // side-effects; safe to invoke after the runtime has completed.
    qCDebug(lcCli) << "RunCommand::execute: computing container allocation and path metrics...";
    const auto allocation = Backend::Scenario::ContainerAllocator::allocate(
        rt.document(), rt.paths());

    QHash<int, Backend::Scenario::PathMetrics> metrics;
    auto *cfg = ctl.getConfigController();
    auto *veh = ctl.getVehicleController();
    if (cfg)
    {
        for (auto *p : rt.paths())
        {
            if (!p || p->getSegments().isEmpty())
                continue;
            const auto mode =
                p->getSegments().first()->getMode();
            const auto inputs =
                Backend::Scenario::PathMetricsCalculator::gatherInputs(
                    mode, *cfg, veh);
            const int count =
                allocation.byPathId.value(p->getPathId()).size();
            metrics.insert(
                p->getPathId(),
                Backend::Scenario::PathMetricsCalculator::compute(
                    p->totalEstimatedLength(),
                    p->totalEstimatedTravelTime(),
                    mode, inputs, count));
        }
        qCDebug(lcCli) << "RunCommand::execute: computed metrics for"
                       << metrics.size() << "paths";
    }
    else
    {
        qCWarning(lcCli) << "RunCommand::execute: ConfigController is null — skipping per-path metrics";
    }

    for (const auto &fmt : rt.document().output.formats)
    {
        QString werr;
        bool    okWrite = true;
        if (fmt == QLatin1String("json"))
        {
            const QString filePath =
                QDir(outDir).filePath(QStringLiteral("results.json"));
            qCDebug(lcCli) << "RunCommand::execute: writing JSON results to" << filePath;
            JsonResultsWriter jw;
            okWrite = jw.write(filePath, results, &werr,
                               metrics, allocation.keyByPathId);
        }
        else if (fmt == QLatin1String("csv"))
        {
            const QString filePath =
                QDir(outDir).filePath(QStringLiteral("results.csv"));
            qCDebug(lcCli) << "RunCommand::execute: writing CSV results to" << filePath;
            CsvResultsWriter cw;
            okWrite = cw.write(filePath, results, &werr);
        }
        else
        {
            qCWarning(lcCli) << "RunCommand::execute: unknown output format" << fmt;
            streamToOr(m_err, stderr,
                       QStringLiteral(
                           "run: ignoring unknown output format '%1'\n")
                           .arg(fmt));
            continue;
        }
        if (!okWrite)
        {
            qCCritical(lcCli) << "RunCommand::execute: writer" << fmt
                              << "failed —" << werr;
            streamToOr(m_err, stderr,
                       QStringLiteral("run: writer '%1': %2\n")
                           .arg(fmt, werr));
        }
        else
        {
            qCInfo(lcCli) << "RunCommand::execute: results written, format =" << fmt;
        }
    }

    qCInfo(lcCli) << "RunCommand::execute: finished — exit code = Success";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
