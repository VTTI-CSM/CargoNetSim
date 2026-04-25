#pragma once

#include <QJsonObject>
#include <QList>
#include <QVariantMap>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h" // for PathTerminal value type

namespace CargoNetSim
{
namespace Backend
{
class Terminal;
class PathSegment;
namespace Scenario
{

/**
 * @brief Pure-function home for terminal-level cost / delay math.
 *
 * This namespace exists for **separation of concerns**, not for
 * multi-consumer DRY. Keeping the math pure and outside of
 * `ResultsExtractor` buys three things:
 *
 *   - **Testability.** Every function takes a JSON configuration and
 *     returns a scalar. Unit tests don't need to stand up a
 *     `ScenarioRuntime`, RabbitMQ clients, an event loop, or any
 *     orchestration — just feed config in, check the number out.
 *   - **Readability.** `ResultsExtractor` stays focused on *what*
 *     happens (iterate paths → dispatch by mode → aggregate results);
 *     the ~200 lines of *how* live here. Mixing them would bury the
 *     pipeline under domain math.
 *   - **Future reuse.** Any CLI-side "what-if" analyzer or report
 *     generator can call these directly without pulling in the
 *     ResultsExtractor pipeline.
 *
 * Stateless — no QObject, no signals, no cached state. Each function
 * takes a scenario/terminal JSON configuration object and returns a
 * single scalar result.
 *
 * Active consumer: `ResultsExtractor`. Per-function "VERBATIM from ..."
 * breadcrumbs document the math's extraction lineage and should not be
 * edited without checking behavior against the original source.
 */
namespace TerminalCostMath
{

/**
 * @brief Mean/expected dwell time in seconds for a terminal, based on the
 *        `dwell_time.method` and `dwell_time.parameters` fields of the
 *        per-terminal JSON config.
 *
 * Supported methods (case-insensitive):
 *   - "gamma"       → shape * scale
 *   - "exponential" → scale
 *   - "normal"      → mean
 *   - "lognormal"   → exp(mean + sigma^2 / 2)
 *   - any other     → gamma with shape=2.0, scale=86400.0
 *
 * Missing `dwell_time` section → 0.0. Defaults mirror the original
 * SimulationValidationWorker body exactly.
 */
double dwellTime(const QJsonObject &config);

/**
 * @brief Expected customs delay given the `customs.probability` and
 *        `customs.delay_mean` fields. Writes `probability * delay_mean`
 *        into @p customsDelay when both are positive; leaves
 *        @p customsCost untouched (that's directCosts's job).
 *
 * @return true when customs actually applied (both prob and delay
 *         positive); false otherwise.
 */
bool customs(const QJsonObject &config,
             double            &customsDelay,
             double            &customsCost);

/**
 * @brief Direct monetary cost at a terminal, summing:
 *          - `cost.fixed_fees`
 *          - `cost.customs_fees` (only if @p customsApplied is true)
 *          - `cost.risk_factor` × nominal container value (1.0)
 *
 * Missing `cost` section → 0.0. Matches the original SVW body exactly.
 */
double directCosts(const QJsonObject &config, bool customsApplied);

/**
 * @brief Total terminal cost for a single terminal, combining dwell-time,
 *        customs, and direct-cost math with weight multipliers.
 *
 * Selects per-mode weights from @p costFunctionWeights using the integer
 * string key of @p mode (e.g. "0" for Ship). Falls back to the "default"
 * entry when @p mode is Any (-1) or the mode key is absent.
 * Null terminal → 0.0.
 */
double singleTerminalCost(
    CargoNetSim::Backend::Terminal *terminal,
    const QVariantMap              &costFunctionWeights,
    int                             containerCount,
    TransportationTypes::TransportationMode
        mode = TransportationTypes::TransportationMode::Any);

/**
 * @brief Sum of per-segment terminal costs across a full path.
 *
 * Reads `segment.getAttributes()["estimated_cost"].previousTerminalCost`
 * and `nextTerminalCost`, then applies the first/middle/last split
 * (first segment counts full previous; last counts full next; intermediate
 * splits halve both). Empty segment list → 0.0. @p terminals and
 * @p weights are parameters for API parity with the original SVW body and
 * its commented-out mode-change loop; they are not read by the active
 * code path.
 */
double totalTerminalCosts(
    const QList<CargoNetSim::Backend::PathSegment *> &segments,
    const QList<CargoNetSim::Backend::PathTerminal>  &terminals,
    const QVariantMap                                &weights,
    int                                               containerCount);

} // namespace TerminalCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
