#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace CargoNetSim {
namespace Cli {

/**
 * @brief One running-`run` snapshot persisted to disk.
 *
 * Used by `cargonetsim-cli status` (Task 15) and `stop` (Task 16) to
 * discover live `run` processes (Task 17): each running `run` writes
 * one of these to `runtimeStateDir()` on startup, removes it on exit,
 * and exposes a `ControlChannel` server at `socketName`.
 */
struct RuntimeStateEntry
{
    qint64    pid = -1;
    QString   socketName;
    QString   scenarioPath;
    QDateTime startedAt;
};

/**
 * @brief File-system registry of live `cargonetsim-cli run` processes.
 *
 * Plan 5 Task 10 / spec §8.4. Provides the static lifecycle hooks
 * `RunCommand` calls on startup (`write`) and exit (`removeForPid`),
 * and the discovery primitive `status` / `stop` use (`scan`).
 *
 * State files live under `runtimeStateDir()` named
 * `cargonetsim-cli-<pid>.state` and contain a single JSON object with
 * the fields of `RuntimeStateEntry`.
 *
 * Stale entries (from crashed `run` processes that never reached the
 * cleanup path) are tolerated: `scan()` returns them; downstream
 * consumers attempt `ControlChannel` connections and ignore the ones
 * that fail. The file leaks until the next `run` on the same PID
 * overwrites it or until the OS clears the temp dir.
 *
 * Stateless: all methods are static. No thread-safety provided —
 * concurrent writers on different PIDs touch different files
 * (`QSaveFile` is atomic per file).
 */
class RuntimeStateFile
{
public:
    /// Resolve the directory that holds runtime state files. Honors
    /// the `CARGONETSIM_CLI_RUNTIME_DIR` env override (used by tests
    /// to redirect to a `QTemporaryDir`); otherwise defaults to
    /// `<TempLocation>/cargonetsim-cli/`. Always returns a directory
    /// path with no trailing slash; pair with `QDir::filePath()` to
    /// build child paths reliably.
    static QString runtimeStateDir();

    /// Write @p entry to `cargonetsim-cli-<pid>.state` atomically via
    /// `QSaveFile`. Creates the runtime dir if it does not exist.
    /// Returns false on I/O failure with a human-readable reason in
    /// @p err (caller can pass nullptr).
    static bool write(const RuntimeStateEntry &entry, QString *err);

    /// Delete the state file for @p pid. Returns true on success.
    /// Missing files are not an error from the caller's perspective
    /// (return false but `removeForPid` is meant to be best-effort
    /// cleanup at exit).
    static bool removeForPid(qint64 pid);

    /// Enumerate every state file in `runtimeStateDir()`. Malformed
    /// files (unparseable JSON, missing required fields) are silently
    /// skipped — `scan` is used by crash-recovery code paths and must
    /// not abort on a single bad entry.
    static QList<RuntimeStateEntry> scan();
};

} // namespace Cli
} // namespace CargoNetSim
