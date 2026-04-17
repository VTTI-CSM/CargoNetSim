#include "ConnectionsCommand.h"

#include <QCoreApplication>
#include <QIODevice>
#include <QString>
#include <QTimer>

#include <csignal>
#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// Single source of truth for the one-line-per-server output format.
/// Negative widths in `QString::arg` left-align within the column;
/// width 12 accommodates every documented server name (ship, train,
/// truck, terminal) with a two-space gutter.
QString formatStatus(
    const Backend::Scenario::ServerStatusProbe::ServerStatus &s)
{
    return QStringLiteral("%1  %2  consumers=%3\n")
        .arg(s.server, -12)
        .arg(s.connected ? QStringLiteral("connected")
                         : QStringLiteral("disconnected"),
             -12)
        .arg(s.hasConsumers ? 1 : 0);
}

} // namespace

ConnectionsCommand::ConnectionsCommand(PollerFn poller, QIODevice *outSink)
    : m_poller(std::move(poller))
    , m_out(outSink)
{
    // Lazy-init the default poller so the ServerStatusProbe is not
    // constructed at command-construction time — polling only happens
    // when `execute()` runs, which is what we want: a command
    // registered by the dispatcher but never invoked does not touch
    // the controller singleton.
    if (!m_poller)
    {
        m_poller = [] {
            Backend::Scenario::ServerStatusProbe probe;
            return probe.pollAll();
        };
    }
}

int ConnectionsCommand::execute(const QStringList &args)
{
    qCDebug(lcCli) << "ConnectionsCommand::execute: entry, args =" << args;

    const bool watch = args.contains(QStringLiteral("--watch"));
    qCDebug(lcCli) << "ConnectionsCommand::execute: watch =" << watch;

    // One render pass: poll, format, emit, compute exit code. Shared
    // between the one-shot path and every watch tick.
    const auto renderOnce = [this]() -> int {
        const auto statuses = m_poller();
        qCDebug(lcCli) << "ConnectionsCommand::renderOnce: polled"
                       << statuses.size() << "server(s)";
        QString    buffer;
        bool       allGood = true;
        for (const auto &s : statuses)
        {
            buffer += formatStatus(s);
            if (!s.connected)
            {
                qCDebug(lcCli) << "ConnectionsCommand::renderOnce: server"
                               << s.server << "is disconnected";
                allGood = false;
            }
        }
        streamToOr(m_out, stdout, buffer);
        return allGood ? static_cast<int>(ExitCode::Success)
                       : static_cast<int>(ExitCode::ServerDisconnected);
    };

    if (!watch)
    {
        const int code = renderOnce();
        qCDebug(lcCli) << "ConnectionsCommand::execute: one-shot done, exit code =" << code;
        return code;
    }

    // --watch: install a minimal SIGINT handler so Ctrl+C drives the
    // event loop out of `exec()` with a clean exit code rather than
    // killing the process abruptly. Scoped to the watch branch so the
    // one-shot path leaves the default SIGINT behaviour untouched.
    qCInfo(lcCli) << "ConnectionsCommand::execute: entering --watch mode (1 s interval)";
    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout,
                     [renderOnce]() { renderOnce(); });
    timer.start(1000);
    renderOnce();                       // first line immediately
    return QCoreApplication::exec();    // 0 when quit() is called
}

} // namespace Cli
} // namespace CargoNetSim
