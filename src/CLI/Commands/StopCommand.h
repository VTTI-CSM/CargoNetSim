#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli stop [--pid N]` — ask a running
 *        `cargonetsim-cli run` process to shut down cleanly.
 *
 * Plan 5 Task 16. Iterates Task 10's `RuntimeStateFile::scan()`,
 * resolves a target PID, and sends `{"op":"stop"}` over Task 9's
 * `ControlChannelClient` with a 10-second acknowledgement window
 * (longer than `status`'s 500 ms because graceful simulation tear-
 * down may take several seconds).
 *
 * Target-PID resolution:
 *   * Zero entries in scan            → `NoRunningSim` + diagnostic.
 *   * Exactly one entry, no `--pid`   → that entry.
 *   * Exactly one entry with `--pid`  → must match that entry's pid,
 *                                       otherwise `NoRunningSim`.
 *   * Multiple entries, no `--pid`    → `BadArgs` + diagnostic listing
 *                                       every running PID so the user
 *                                       can retry.
 *   * Multiple entries with `--pid`   → pick the matching entry, or
 *                                       `NoRunningSim` + "not found".
 *   * `--pid` followed by a missing,
 *     non-numeric, or non-positive
 *     value                           → `BadArgs` + diagnostic.
 *
 * Success (`ExitCode::Success = 0`) is reserved for the case where
 * the server's reply object contains `"ok": true`. Any other outcome
 * (no reply, reply with `"ok": false`, pid not in scan, server
 * refused) is reported as `NoRunningSim` with a diagnostic so the
 * user understands why. A refining ExitCode could be added later;
 * existing shell scripts already check for exact-zero = "stopped".
 *
 * Output sink: optional `QIODevice *` stderr override. `stop` never
 * writes to stdout.
 */
class StopCommand : public Subcommand
{
public:
    explicit StopCommand(QIODevice *errSink = nullptr);

    int execute(const QStringList &args) override;

private:
    QIODevice *m_err;  // not owned; nullptr → write to stderr
};

} // namespace Cli
} // namespace CargoNetSim
