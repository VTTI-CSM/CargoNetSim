#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli validate <scenario.yml>` — parse a scenario
 *        YAML file, run the scenario validator, print every issue to
 *        stderr, and exit with a status code reflecting whether any
 *        errors were found.
 *
 * Plan 5 Task 12. Exit codes:
 *   * `ExitCode::Success (0)`        — file parsed, zero errors
 *     (warnings are reported but do not change the exit code).
 *   * `ExitCode::ValidationFailed (2)` — file failed to parse OR at
 *     least one validator issue has `Severity::Error`.
 *   * `ExitCode::BadArgs (64)`       — no scenario path supplied.
 *
 * Issue output format (one line per issue):
 * @code
 *   ERROR <path>: <message>
 *   WARN  <path>: <message>
 * @endcode
 * where `<path>` is the dotted document address
 * (e.g. `terminals[T1]`) produced by the validator.
 *
 * Output sink: optional `QIODevice *` constructor parameter is a
 * test-injection seam — production wiring uses the default
 * constructor which writes to stderr. The injected device must
 * outlive the command instance.
 */
class ValidateCommand : public Subcommand
{
public:
    explicit ValidateCommand(QIODevice *errSink = nullptr);
    int execute(const QStringList &args) override;

private:
    QIODevice *m_err;  // not owned; nullptr → write to stderr
};

} // namespace Cli
} // namespace CargoNetSim
