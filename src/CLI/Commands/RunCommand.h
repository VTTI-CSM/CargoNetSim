#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli run <scenario.yml>` — full headless
 *        simulation pipeline.
 *
 * Plan 5 Task 17. This is the orchestrator — every subsystem touched
 * by `run` is implemented elsewhere and composed here:
 *   1. `ScenarioSerializer::fromYaml`      — parse the YAML.
 *   2. `ScenarioValidator::validate`       — surface every issue to
 *      stderr via the shared `formatValidationIssues` helper.
 *   3. `CargoNetSimController::initialize` + `startAll` + wait for
 *      the `allClientsReady` signal (30 s budget).
 *   4. `ScenarioRuntime::load`             — validate + apply.
 *   5. `Backend::Scenario::PathDiscovery`  — produce the top-N paths.
 *   6. `RuntimeStateFile::write` (Task 10) — make this process
 *      visible to sibling `status` / `stop` commands.
 *   7. `ControlChannelServer` (Task 9)     — accept `{"op":"status"}`
 *      and `{"op":"stop"}` from those siblings.
 *   8. `ProgressReporter` (Task 8)         — rate-limited stderr
 *      ticks driven by `ScenarioRuntime::progressChanged`.
 *   9. `ScenarioRuntime::startSimulation`  — spawn the executor;
 *      block the calling thread on a local `QEventLoop` until
 *      `completed` or `failed`.
 *  10. `JsonResultsWriter` / `CsvResultsWriter` (Tasks 6 + 7) —
 *      emit `results.{json,csv}` under the YAML's `output.directory`.
 *
 * Argument contract (plan 5 preamble, spec §8.3): `run` takes
 * **exactly one** positional scenario YAML file. Any other argv shape
 * (zero args, ≥ 2 args, any `--flag`) returns `ExitCode::BadArgs`.
 * Every simulation parameter — output directory, end time, origins,
 * etc. — is read from the YAML itself.
 *
 * Cleanup: every resource (controller startup, state file,
 * RabbitMQ connection, IPC listener) is released via `qScopeGuard`
 * on every exit path, success or failure. No stale state files or
 * lingering sockets after `run` returns.
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
    QIODevice *m_err;  // not owned; nullptr → write to stderr
};

} // namespace Cli
} // namespace CargoNetSim
