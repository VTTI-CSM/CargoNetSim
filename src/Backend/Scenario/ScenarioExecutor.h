#pragma once

#include <QList>
#include <QObject>
#include <QVector>
#include <QString>
#include <atomic>

#include "ScenarioExecutionResult.h"
#include "PathSimulationResult.h"
#include "ExecutionPlanTypes.h"

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
 * @brief End-to-end backend-pure multimodal execution driver.
 *
 * Builds an execution plan, coordinates per-path segment activation,
 * dispatches execution waves through persistent simulator sessions,
 * and extracts typed execution results. This is the production
 * execution entry point replacing the older mode-batched orchestration
 * pipeline.
 *
 * Decoupling: this class does NOT depend on ScenarioRuntime. Inputs are
 * injected via setters so the executor stays independently testable and
 * the runtime can simply construct + inject + invoke without circular
 * dependencies. Both the GUI and the CLI end up driving through the
 * runtime, never directly.
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

    /** @brief Set the stable prepared-path identities aligned to setPaths(). */
    void setPathIdentities(const QVector<QString> &pathIdentities);

    /** @brief Set how selected path demand is assigned for this run. */
    void setDemandPolicy(ExecutionDemandPolicy demandPolicy);

    /**
     * @brief Set whether selected paths share simulator state or run as
     *        isolated what-if alternatives.
     */
    void setIsolationPolicy(ExecutionIsolationPolicy isolationPolicy);

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
    QList<PathSimulationResult> results() const
    {
        return m_executionResults.summaryResults();
    }

    const ScenarioExecutionResultSet &executionResults() const
    {
        return m_executionResults;
    }

    const ScenarioExecutionPlan &executionPlan() const
    {
        return m_executionPlan;
    }

    const ExecutionLedger &executionLedger() const
    {
        return m_executionLedger;
    }

    const QVector<DispatchableSegmentRef> &dispatchableSegments() const
    {
        return m_dispatchableSegments;
    }

    void requestStop();
    void requestPause();
    void requestResume();

signals:
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void progressChanged(double currentTime, double percent);
    void progressSnapshotChanged(
        double currentTime,
        const ExecutionProgressSnapshot &snapshot);
    void succeeded();
    void failed(const QString &message);
    void finished();

private:
    /**
     * @brief Validates inputs are present and the document carries
     *        origin containers + paths to simulate. On failure sets
     *        @p err and returns false.
     */
    bool validateInputs(QString *err);
    bool runIsolatedAlternativeExecutions();

    const ScenarioDocument             *m_document = nullptr;
    const ScenarioRegistry             *m_registry = nullptr;
    QList<CargoNetSim::Backend::Path *> m_paths;
    QVector<QString>                    m_pathIdentities;
    ExecutionDemandPolicy               m_demandPolicy =
        ExecutionDemandPolicy::AllocatedOnly;
    ExecutionIsolationPolicy            m_isolationPolicy =
        ExecutionIsolationPolicy::SharedSimulatorState;
    ScenarioExecutionResultSet          m_executionResults;
    ScenarioExecutionPlan               m_executionPlan;
    ExecutionLedger                     m_executionLedger;
    QVector<DispatchableSegmentRef>     m_dispatchableSegments;
    std::atomic_bool                    m_stopRequested{false};
    std::atomic_bool                    m_pauseRequested{false};
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
