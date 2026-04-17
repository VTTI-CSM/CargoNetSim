#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli status [--json]` — discover every live
 *        `cargonetsim-cli run` process and report its simulation
 *        progress.
 *
 * Plan 5 Task 15. Composes Task 10's `RuntimeStateFile::scan()` for
 * discovery and Task 9's `ControlChannelClient::send(name, {"op":
 * "status"})` for per-process queries. 500 ms probe timeout per
 * entry (bounded by `ControlChannelClient`'s own deadline).
 *
 * Output modes:
 *   * **Human** (default) — one block per live simulation with
 *     right-padded label columns (`state`, `current_time`,
 *     `end_time`, `progress`).
 *   * **`--json`** — emit a JSON array; each element is the server's
 *     response merged with the state file's `pid`, `scenario_path`,
 *     and `started_at` metadata. Suitable for shell pipelines.
 *
 * Exit codes:
 *   * `Success (0)`        — at least one state-file entry responded
 *     with a valid status object.
 *   * `NoRunningSim (4)`   — no state files found OR every state
 *     file pointed at an unresponsive / stale socket.
 *
 * Output sink: optional `QIODevice *` constructor parameter is a
 * test-injection seam. Production writes to real stdout.
 *
 * Discovery is read-only; `StatusCommand` never removes stale state
 * files. Callers that need cleanup use `StopCommand` (Task 16) or
 * let the next `run` invocation reclaim the name.
 */
class StatusCommand : public Subcommand
{
public:
    explicit StatusCommand(QIODevice *outSink = nullptr);

    int execute(const QStringList &args) override;

private:
    QIODevice *m_out;  // not owned; nullptr → write to stdout
};

} // namespace Cli
} // namespace CargoNetSim
