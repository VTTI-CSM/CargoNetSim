#pragma once

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Stable exit-code contract for `cargonetsim-cli`.
 *
 * The integer values of this enum are a published contract — documented
 * in `cargonetsim-cli --help` (Task 11) and depended on by CI pipelines
 * and shell scripts. They MUST NOT change without a major version bump.
 * Per Plan 5 spec §8.5.
 *
 * `Success = 0` mirrors the POSIX success convention; `BadArgs = 64`
 * mirrors `EX_USAGE` from `sysexits.h`. The 1–5 range is reserved for
 * CargoNetSim-specific failure modes and is exhaustive — any new exit
 * reason belongs on this list, not as an ad-hoc integer at a call site.
 *
 * This is a pure contract header: no functions, no state, no includes
 * beyond the pragma. One source of truth for the mapping; the lock-test
 * lives in `tests/CLI/ExitCodesTest.cpp`.
 */
enum class ExitCode : int
{
    Success            = 0,   ///< Subcommand completed normally.
    RunFailed          = 1,   ///< `run` subcommand: simulation failed at runtime.
    ValidationFailed   = 2,   ///< `validate`/`run`/`preview`: scenario YAML has errors.
    ConnectTimeout     = 3,   ///< `run`: simulators unreachable within timeout.
    NoRunningSim       = 4,   ///< `status`/`stop`: no live simulation to address.
    ServerDisconnected = 5,   ///< `connections`: at least one simulator disconnected.
    BadArgs            = 64,  ///< Misuse of command line (EX_USAGE).
};

} // namespace Cli
} // namespace CargoNetSim
