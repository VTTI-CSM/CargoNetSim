#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "Backend/Models/Path.h"
#include "Backend/Scenario/PathKey.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/PathSimulationResult.h"

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Emit a `results.json` file given the per-path cost breakdown
 *        produced by `Backend::Scenario::ScenarioExecutor`.
 *
 * Output shape (schema frozen — published contract):
 * @code{.json}
 * {
 *   "schema_version": 2,
 *   "generated_at":   "2026-04-12T14:30:00Z",
 *   "paths": [
 *     {
 *       "path_id":        1,
 *       "total_cost":     123.45,
 *       "edge_costs":     100.0,
 *       "terminal_costs": 23.45,
 *
 *       // ---- Optional, present iff PathKey supplied for path_id ----
 *       "origin":      "O1",
 *       "destination": "DA",
 *       "rank":        0,
 *
 *       // ---- Optional, present iff valid PathMetrics supplied ------
 *       "metrics": {
 *         "container_count": 6,
 *         "vehicles_needed": 1,
 *         "distance_km":     120.0,
 *         "travel_time_h":   2.0,
 *         "fuel_type":       "diesel_1",
 *         "per_vehicle": {
 *           "fuel": 2.5, "energy_kwh": 3000.0,
 *           "carbon_t": 0.804, "risk": 0.02
 *         },
 *         "per_container": {
 *           "fuel": 0.417, "energy_kwh": 500.0,
 *           "carbon_t": 0.134, "risk": 0.003
 *         }
 *       },
 *       "segments": [
 *         {
 *           "segment_id":   "T1→T2",
 *           "mode":         "rail",
 *           "from":         "T1",
 *           "to":           "T2",
 *           "estimated": {
 *             "distance_m":    50000.0,
 *             "travel_time_s": 2040.0,
 *             "energy_kwh":    980.0,
 *             "carbon_t":      0.45,
 *             "risk":          0.006
 *           }
 *         }
 *       ]
 *     },
 *     ...
 *   ]
 * }
 * @endcode
 *
 * `generated_at` is always UTC (the `Z` suffix is mandatory in the
 * output); the writer treats its input as a pure projection of the
 * supplied list and stamps the current UTC clock at serialization.
 *
 * Writes are atomic via `QSaveFile` — the target file is either the
 * previous contents or the new contents; a crash mid-write leaves no
 * partial output. The writer does NOT create the parent directory;
 * that is the caller's responsibility (see `RunCommand` Task 17 which
 * does `QDir().mkpath(outputDir)` before calling here).
 *
 * Per-segment `actual_values` / `actual_cost` are already written onto
 * each `Path`'s segments by `ResultsExtractor` (Plan 3); reading them
 * back into the JSON output is a future extension and intentionally
 * out of scope for this task.
 *
 * The optional `metrics` and `origin`/`destination`/`rank` fields are
 * emitted when the caller passes populated `metrics` / `keys` maps
 * keyed by canonical path identity. This avoids collisions when
 * multiple origin/destination pairs legitimately reuse the same local
 * `path_id`. The `metrics` block is the preview/discovery view of a
 * path; execution allocation stays in `effective_container_count`.
 *
 * Stateless: one instance per invocation. No thread-safety concerns —
 * writers do not share state.
 */
class JsonResultsWriter
{
public:
    /**
     * @param outputPath  Full destination path including filename.
     * @param results     Per-path breakdown; may be empty (produces a
     *                    valid file with an empty `paths` array).
     * @param err         On failure, filled with a human-readable
     *                    reason. Caller can pass nullptr.
     * @param metrics     Optional per-path metrics keyed by canonical
     *                    path identity.
     *                    When a result's key is present AND the value's
     *                    `valid` flag is true, a nested `metrics`
     *                    object is emitted.
     * @param keys        Optional per-path (origin, destination, rank)
     *                    tuples keyed by canonical path identity. When
     *                    present, they are emitted as top-level fields
     *                    on the path object.
     * @param paths       Optional list of Path pointers. When a
     *                    matching Path is found for a result's canonical
     *                    key,
     *                    a `segments` array is emitted containing each
     *                    segment's estimated metrics.
     * @return `true` on success, `false` on any I/O failure.
     */
    bool write(
        const QString &outputPath,
        const QList<CargoNetSim::Backend::Scenario::PathSimulationResult>
                      &results,
        QString       *err,
        const QHash<QString, CargoNetSim::Backend::Scenario::PathMetrics>
                      &metrics = {},
        const QHash<QString, CargoNetSim::Backend::Scenario::PathKey>
                      &keys    = {},
        const QList<CargoNetSim::Backend::Path *>
                      &paths   = {});
};

} // namespace Cli
} // namespace CargoNetSim
