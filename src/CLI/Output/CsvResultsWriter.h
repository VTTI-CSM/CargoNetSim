#pragma once

#include <QList>
#include <QString>

#include "Backend/Scenario/PathSimulationResult.h"

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Emit a `results.csv` file given the per-path cost breakdown
 *        produced by `Backend::Scenario::ScenarioExecutor`.
 *
 * Output shape (schema frozen — published contract):
 * @code
 * path_id,total_cost,edge_costs,terminal_costs
 * 0,123.450000,100.000000,23.450000
 * 1,456.000000,400.000000,56.000000
 * @endcode
 *
 * Contract details:
 *   * **Line endings are always LF**, on every platform — spec §9.
 *     The file is opened in binary (non-text) mode precisely so Qt
 *     does not translate `\n` into `\r\n` on Windows.
 *   * **Encoding is UTF-8 with no byte-order-mark.** The content is
 *     pure ASCII today, but the encoding is fixed so downstream tools
 *     never have to sniff.
 *   * **Numeric columns are fixed-point with six decimal places**
 *     (`%.6f`). Stable enough for spreadsheet consumers to diff.
 *
 * Writes are atomic via `QSaveFile` — the target file is either the
 * previous contents or the new contents; a crash mid-write leaves no
 * partial output. The writer does NOT create the parent directory;
 * that is the caller's responsibility.
 *
 * Stateless: one instance per invocation. No thread-safety concerns —
 * writers do not share state.
 *
 * Mirrors `JsonResultsWriter` by signature; see that class for the
 * sibling JSON-format output.
 */
class CsvResultsWriter
{
public:
    /**
     * @param outputPath  Full destination path including filename.
     * @param results     Per-path breakdown; may be empty (produces a
     *                    valid file containing only the header line).
     * @param err         On failure, filled with a human-readable
     *                    reason. Caller can pass nullptr.
     * @return `true` on success, `false` on any I/O failure.
     */
    bool write(
        const QString &outputPath,
        const QList<CargoNetSim::Backend::Scenario::PathSimulationResult>
                      &results,
        QString       *err);
};

} // namespace Cli
} // namespace CargoNetSim
