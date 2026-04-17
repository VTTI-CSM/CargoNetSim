#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <memory>

#include "PathSimulationResult.h"
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
 *   3. setPaths(paths)                    — GUI or CLI provides paths
 *   4. startSimulation()                  — spawn executor on worker
 *      thread, return immediately; result arrives via `completed`/`failed`
 *   5. stop()/pause()/resume()            — forward to CargoNetSimController
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

    /** @brief Caller supplies the paths to simulate (GUI: checked rows
     *         from ShortestPathTable; CLI: discovered paths). Must be
     *         set before startSimulation. */
    void setPaths(const QList<CargoNetSim::Backend::Path *> &paths);

    /** @brief Spawn the executor on the worker thread. Returns immediately;
     *         progress via signals. Fails if load() hasn't run, or if a
     *         prior simulation is still active. */
    bool startSimulation();

    void stop();
    void pause();
    void resume();

    double currentTime() const;
    double progress() const;
    bool   isRunning() const;

    const ScenarioDocument &document() const;
    ScenarioDocument       &document();
    const ScenarioRegistry &registry() const { return m_registry; }
    ScenarioRegistry       &registry()       { return m_registry; }

    QList<PathSimulationResult> results() const { return m_lastResults; }

    /** @brief Plan 8.2: completion handler reads actual segment
     *         attributes through these non-owning path pointers. */
    const QList<CargoNetSim::Backend::Path *> &paths() const
    {
        return m_paths;
    }

signals:
    void progressChanged(double currentTime, double percent);
    void completed();
    void failed(const QString &message);
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);

private slots:
    void onStepCompleted(double currentTime, double progress);
    void onExecutorFinished();

private:
    std::unique_ptr<ScenarioDocument>   m_document;
    ScenarioRegistry                    m_registry;
    QThread                            *m_workerThread = nullptr;
    ScenarioExecutor                   *m_executor     = nullptr;
    QList<CargoNetSim::Backend::Path *> m_paths;
    QList<PathSimulationResult>         m_lastResults;
    bool                                m_loaded       = false;
    double                              m_endTime      = 0.0;
    double                              m_lastTime     = 0.0;
    double                              m_lastProgress = 0.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
