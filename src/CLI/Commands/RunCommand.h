#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QString>

#include "Backend/CliApi/ResultsApi.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "CLI/ExitCodes.h"
#include "CLI/Subcommand.h"

class QIODevice;
class RunCommandTest;

namespace CargoNetSim {
namespace Backend {
class Path;
namespace Scenario {
class ScenarioDocument;
}
}
namespace Cli {

/**
 * @brief `cargonetsim-cli run <scenario.yml>` — full headless
 *        simulation pipeline.
 *
 * Plan 5 Task 17. This is the orchestrator — every subsystem touched
 * by `run` is implemented elsewhere and composed here:
 *   1. `ScenarioLoadService::parseAndValidateYaml`
 *      — parse the YAML and collect validation issues.
 *   2. `ScenarioValidator::validate`       — surface every issue to
 *      stderr via the shared `formatValidationIssues` helper.
 *   3. `CargoNetSimController::initialize` + `startAll`.
 *   4. `ScenarioLoadService::loadValidatedDocument`
 *      — apply the validated scenario into a runtime.
 *   5. `PreparedPathService`               — produce the top-N paths.
 *   6. `ProgressReporter`                  — rate-limited stderr
 *      ticks driven by runtime progress snapshots.
 *   7. `ScenarioRuntime::statusMessage`    — direct stage updates
 *      streamed to the terminal during the blocking run.
 *   8. `SimulationRunService::validateAndStart`
 *      — preflight + spawn the executor; then block the calling
 *      thread on a local `QEventLoop` until `completed` or `failed`.
 *   9. `JsonResultsWriter` / `CsvResultsWriter` (Tasks 6 + 7) —
 *      emit `results.{json,csv}` under the YAML's `output.directory`.
 *
 * Argument contract: `run` takes exactly one positional scenario YAML
 * file. `--all` selects every prepared candidate path. `--paths`
 * selects a comma-separated subset by the 1-based presentation index
 * shown by `cargonetsim-cli discover`; the command translates those
 * indexes to stable prepared-path identities before simulation.
 * Every selected alternative executes as an isolated what-if run under
 * duplicate-demand comparison policy. A bare scenario path is treated
 * as an alias for `--all`.
 *
 * Cleanup: controller startup is released via `qScopeGuard` on every
 * exit path, success or failure.
 *
 * Output sink: the optional `QIODevice *` constructor parameter
 * receives every stderr write — diagnostics, validator issues, and
 * `ProgressReporter` ticks (the reporter is constructed with the
 * same sink). Production wiring passes nullptr (real stderr).
 */
class RunCommand : public Subcommand
{
public:
    explicit RunCommand(QIODevice *errSink = nullptr);

    int execute(const QStringList &args) override;

private:
    friend class ::RunCommandTest;

    struct WriterHooks
    {
        std::function<bool(const QString &)> mkpath;
        std::function<bool(
            const QString &,
            const QList<CargoNetSim::Backend::Scenario::PathSimulationResult> &,
            QString *,
            const QHash<QString, CargoNetSim::Backend::Scenario::PathMetrics> &,
            const QHash<QString, CargoNetSim::Backend::Scenario::PathKey> &,
            const QList<CargoNetSim::Backend::Path *> &)>
            writeJson;
        std::function<bool(
            const QString &,
            const QList<CargoNetSim::Backend::Scenario::PathSimulationResult> &,
            QString *)>
            writeCsv;
    };

    static WriterHooks defaultWriterHooks();

    int writeOutputs(
        const CargoNetSim::Backend::Scenario::ScenarioDocument &doc,
        const QList<CargoNetSim::Backend::Scenario::PathSimulationResult>
            &results,
        const QList<CargoNetSim::Backend::Path *> &paths,
        const QHash<QString, CargoNetSim::Backend::Scenario::PathMetrics>
            &predictedMetricsByCanonicalPath,
        const QHash<QString, CargoNetSim::Backend::Scenario::PathKey>
            &pathKeysByCanonicalPath,
        const WriterHooks                         &hooks =
            defaultWriterHooks()) const;

    QIODevice *m_err;  // not owned; nullptr → write to stderr
    bool       m_verbose = false;
};

} // namespace Cli
} // namespace CargoNetSim
