#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "PreparedPathStatus.h"
#include "ExecutionPlanTypes.h"
#include "PathPreparationService.h"
#include "ScenarioExecutionResult.h"
#include "ScenarioRegistry.h"

class QThread;

namespace CargoNetSim
{
namespace Backend
{
class Path;
namespace Scenario
{
class ScenarioDocument;
class ScenarioExecutor;

/**
 * @brief Single runtime handle driven by both the GUI (Plan 4) and the
 *        CLI (Plan 5).
 *
 * Owns the ScenarioDocument, the ScenarioRegistry, the worker QThread,
 * and the ScenarioExecutor. Relays controller + executor signals through
 * its own signals so consumers subscribe to one source of truth.
 *
 * Lifecycle:
 *   1. ctor(unique_ptr<ScenarioDocument>) — claim ownership of the doc
 *   2. load()                             — validate + apply → ready
 *   3. setPreparedPaths(paths)            — backend-owned prepared paths
 *      become the runtime source of truth
 *   4. setSelectedPathKeys(keys)          — GUI or CLI selects a subset by
 *      stable prepared identity
 *   5. startSimulation()                  — spawn executor on worker
 *      thread, return immediately; result arrives via `completed`/`failed`
 *   6. stop()/pause()/resume()            — signal the active executor
 *
 * CLI blocking-wait idiom:
 *     QEventLoop loop;
 *     connect(rt, &ScenarioRuntime::completed, &loop, &QEventLoop::quit);
 *     connect(rt, &ScenarioRuntime::failed,    &loop, &QEventLoop::quit);
 *     rt->startSimulation();
 *     loop.exec();
 */
class ScenarioRuntime : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioRuntime(std::unique_ptr<ScenarioDocument> doc,
                             QObject                          *parent = nullptr);
    ~ScenarioRuntime() override;

    /** @brief Validate then apply the document onto CargoNetSimController.
     *         Populates the registry. Emits `failed` on first error.
     *
     *  Idempotent — calling a second time re-validates and re-applies. */
    bool load();

    /** @brief Backend-owned prepared paths become the runtime source
     *         of truth for later selection and execution. */
    void setPreparedPaths(const PreparedPathSet &preparedPaths);

    /** @brief Re-evaluate prepared-path eligibility from current
     *         backend simulator availability for GUI/CLI rendering. */
    void refreshPreparedPathEligibility();

    /** @brief Select the subset of prepared paths to simulate using the
     *         stable prepared-path identities supplied by A2 discovery.
     *         Returns false and optionally fills @p err if any requested
     *         identity is unknown. */
    bool setSelectedPathKeys(const QVector<QString> &pathKeys,
                             QString               *err = nullptr);

    /** @brief Configure how selected path demand is assigned at execution. */
    void setDemandPolicy(ExecutionDemandPolicy demandPolicy);
    ExecutionDemandPolicy demandPolicy() const
    {
        return m_demandPolicy;
    }

    /** @brief Spawn the executor on the worker thread. Returns immediately;
     *         progress via signals. Fails if load() hasn't run, or if a
     *         prior simulation is still active. */
    bool startSimulation();

    void stop();
    void pause();
    void resume();

    double currentTime() const;
    double progress() const;
    ExecutionProgressSnapshot progressSnapshot() const;
    bool   isRunning() const;

    const ScenarioDocument &document() const;
    ScenarioDocument       &document();
    const ScenarioRegistry &registry() const { return m_registry; }
    ScenarioRegistry       &registry()       { return m_registry; }
    const PreparedPathSet  &preparedPaths() const
    {
        return m_preparedPaths;
    }
    const QHash<QString, PreparedPathEligibility> &preparedPathEligibility()
        const
    {
        return m_preparedPathEligibility;
    }

    /** @brief Validate the current selection against fresh backend
     *         simulator availability. */
    bool validateCurrentSelectionForSimulation(
        QString *err = nullptr) const;

    QList<PathSimulationResult> results() const
    {
        return m_lastExecutionResults.summaryResults();
    }

    const ScenarioExecutionResultSet &executionResults() const
    {
        return m_lastExecutionResults;
    }

    QHash<QString, PathMetrics> actualPathMetrics() const;

    /** @brief Backend-owned selected path view used by executor and post-run
     *         completion handlers. */
    const QList<CargoNetSim::Backend::Path *> &paths() const
    {
        return m_paths;
    }

signals:
    void progressChanged(double currentTime, double percent);
    void progressSnapshotChanged(
        double currentTime,
        const ExecutionProgressSnapshot &snapshot);
    void completed();
    void failed(const QString &message);
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);

private slots:
    void onStepCompleted(double currentTime, double progress);
    void onProgressSnapshotChanged(
        double currentTime,
        const ExecutionProgressSnapshot &snapshot);
    void onExecutorSucceeded();
    void onExecutorFailed(const QString &message);
    void onExecutorFinished();

private:
    enum class TerminalOutcome
    {
        None,
        Succeeded,
        Failed
    };

    void cleanupWorker();

    std::unique_ptr<ScenarioDocument>   m_document;
    ScenarioRegistry                    m_registry;
    PreparedPathSet                     m_preparedPaths;
    QHash<QString, PreparedPathEligibility> m_preparedPathEligibility;
    QThread                            *m_workerThread = nullptr;
    ScenarioExecutor                   *m_executor     = nullptr;
    QList<CargoNetSim::Backend::Path *> m_paths;
    QVector<QString>                    m_selectedPathKeys;
    ExecutionDemandPolicy               m_demandPolicy =
        ExecutionDemandPolicy::AllocatedOnly;
    ScenarioExecutionResultSet          m_lastExecutionResults;
    bool                                m_loaded       = false;
    bool                                m_terminalSignaled = false;
    TerminalOutcome                     m_terminalOutcome = TerminalOutcome::None;
    QString                             m_failureMessage;
    std::optional<double>               m_endTime;
    double                              m_lastTime     = 0.0;
    double                              m_lastProgress = 0.0;
    ExecutionProgressSnapshot           m_lastProgressSnapshot;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
