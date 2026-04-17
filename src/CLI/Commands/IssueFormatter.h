#pragma once

#include <QLatin1String>
#include <QList>
#include <QString>

#include "Backend/Scenario/ValidationIssue.h"

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Format a list of `ValidationIssue`s into a stable
 *        human-readable multi-line block for stderr.
 *
 * Plan 5 Task 14: extracted so every command that surfaces validator
 * output (`validate`, `preview`, eventually `run`'s preflight) emits
 * exactly the same line format. One source of truth.
 *
 * Line format per issue (fixed-width severity prefix + dotted path
 * + message + LF):
 * @code
 *   ERROR  terminals[T1].type: <message>
 *   WARN   simulation.dwell_time.parameters: <message>
 * @endcode
 * Width 7 for the severity column keeps subsequent columns aligned
 * on mixed-severity runs.
 *
 * @param issues   The issues to format (may be empty → returns "").
 * @param anyError Optional out-param: set to true iff at least one
 *                 issue has `Severity::Error`. Callers use this to
 *                 distinguish "warnings only" (exit 0 possible) from
 *                 "hard error present" (exit ≠ 0) without re-walking
 *                 the list.
 */
inline QString formatValidationIssues(
    const QList<Backend::Scenario::ValidationIssue> &issues,
    bool *anyError = nullptr)
{
    using Sev = Backend::Scenario::ValidationIssue::Severity;
    QString buffer;
    if (anyError) *anyError = false;
    for (const auto &iss : issues)
    {
        const char *label =
            iss.severity == Sev::Error   ? "ERROR  "
          : iss.severity == Sev::Warning ? "WARN   "
          :                                "INFO   ";
        buffer += QLatin1String(label);
        buffer += iss.path;
        buffer += QStringLiteral(": ");
        buffer += iss.message;
        buffer += QLatin1Char('\n');
        if (iss.severity == Sev::Error && anyError)
            *anyError = true;
    }
    return buffer;
}

} // namespace Cli
} // namespace CargoNetSim
