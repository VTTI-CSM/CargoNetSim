#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "PathSimulationResult.h"

namespace CargoNetSim
{
namespace Backend
{
class Path;
namespace Scenario
{
class ScenarioDocument;
class ScenarioRegistry;

/**
 * @brief End-to-end backend-pure simulation driver.
 *
 * Wires SimulationRequestBuilder → SimulationOrchestrator →
 * ResultsExtractor in sequence. Replaces SVW::process() (SVW:28-81).
 *
 * Decoupling: this class does NOT depend on ScenarioRuntime. Inputs are
 * injected via setters so the executor stays independently testable and
 * the runtime (Plan 3 Task 24) can simply construct + inject + invoke
 * without circular dependencies. Both the GUI (Plan 4) and the CLI
 * (Plan 5) end up driving through the runtime, never directly.
 *
 * Ownership: pointers passed via setters are non-owning. Caller (the
 * runtime, or test fixtures) keeps document/registry/paths alive for
 * the duration of run().
 */
class ScenarioExecutor : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioExecutor(QObject *parent = nullptr);

    // --- Input setters (called on the orchestrating thread before run) ---

    /** @brief Set the scenario document (originContainers, terminals, etc.). */
    void setDocument(const ScenarioDocument *document);

    /** @brief Set the scenario registry (preview networks, fleet spec). */
    void setRegistry(const ScenarioRegistry *registry);

    /** @brief Set the per-path Path objects to simulate. Caller-owned. */
    void setPaths(const QList<CargoNetSim::Backend::Path *> &paths);

    /**
     * @brief Lifecycle entry point. Validates inputs, runs the
     *        builder/orchestrator/extractor pipeline, emits status,
     *        result, and finished signals. Returns false on failure
     *        with errorMessage describing the cause.
     *
     * Always emits finished() exactly once (success or failure).
     */
    Q_INVOKABLE bool run();

    /** @brief Returns the per-path results from the last run(). */
    QList<PathSimulationResult> results() const { return m_results; }

signals:
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void pathResultReady(const PathSimulationResult &result);
    void finished();

private:
    /**
     * @brief Validates inputs are present and the document carries
     *        origin containers + paths to simulate. On failure sets
     *        @p err and returns false.
     */
    bool validateInputs(QString *err);

    const ScenarioDocument             *m_document = nullptr;
    const ScenarioRegistry             *m_registry = nullptr;
    QList<CargoNetSim::Backend::Path *> m_paths;
    QList<PathSimulationResult>         m_results;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
