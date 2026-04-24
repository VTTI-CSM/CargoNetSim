#pragma once

#include <QList>

#include <functional>

#include "Backend/Scenario/ServerStatusProbe.h"
#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli connections` — print per-server RabbitMQ
 *        connection state, optionally refreshing at 1 Hz.
 *
 * Plan 5 Task 13. Exit codes:
 *   * `ExitCode::Success (0)`            — every polled server reports
 *     `commandAvailable=true` (or the poll yields no servers).
 *   * `ExitCode::ServerDisconnected (5)` — at least one server reports
 *     `commandAvailable=false`.
 *
 * Arguments:
 *   * `--watch` — after the first render, keep polling once per second
 *     and rendering on every tick. Exits cleanly on SIGINT (Ctrl+C).
 *     The watch path commits `QCoreApplication::exec()` so callers
 *     must invoke this command from a process that has not yet entered
 *     the main event loop (every cargonetsim-cli subcommand satisfies
 *     this — the dispatcher is single-shot).
 *
 * Line format (one per server):
 * @code
 *   ship          connected     consumers=1
 *   train         disconnected  consumers=0
 * @endcode
 *
 * Dependency injection:
 *   * `poller`  — callable returning the current status list. Default
 *     (empty std::function) constructs a `ServerStatusProbe` on every
 *     poll, driving `CargoNetSimController::getInstance()`. Tests pass
 *     a lambda that returns canned statuses to exercise output and
 *     exit-code logic without standing up a live controller.
 *   * `outSink` — optional `QIODevice *` stdout override; `nullptr`
 *     writes to real stdout (production default). The injected device
 *     must outlive the command instance.
 */
class ConnectionsCommand : public Subcommand
{
public:
    using PollerFn = std::function<
        QList<Backend::Scenario::ServerStatusProbe::ServerStatus>()>;

    explicit ConnectionsCommand(PollerFn   poller  = {},
                                QIODevice *outSink = nullptr);

    int execute(const QStringList &args) override;

private:
    PollerFn   m_poller;
    QIODevice *m_out;  // not owned; nullptr → write to stdout
};

} // namespace Cli
} // namespace CargoNetSim
