#pragma once

#include <QObject>
#include <QString>

#include "Backend/Application/PreparedPathService.h"

namespace CargoNetSim {
class CargoNetSimController;
namespace Backend {
namespace Scenario {
class ScenarioDocument;
class ScenarioRegistry;
} // namespace Scenario
} // namespace Backend

namespace GUI {

/**
 * @brief Worker-thread wrapper that drives
 *        backend path discovery + preparation off the GUI thread.
 *
 * Plan 5 Task 5 reduced this class to a minimal QObject thread
 * boundary. The worker receives stable backend inputs at initialize-time,
 * performs discovery plus preparation entirely on the worker thread, and
 * re-emits an immutable prepared result back to the GUI thread.
 *
 * Lifecycle (see UtilityFunctions.cpp where this worker is
 * spawned): consumer creates the worker, calls `initialize()`,
 * moves it to a QThread, connects signals, starts the thread
 * which triggers `process()` as its main entry. `finished()` is
 * emitted after `resultReady()` or `error()` so the consumer can
 * tear down the thread and `deleteLater` the worker.
 */
class PathFindingWorker : public QObject
{
    Q_OBJECT

public:
    PathFindingWorker();

    /// Bind the worker to the backend discovery/preparation inputs.
    /// Must be called before `process()`.
    void initialize(
        const Backend::Scenario::ScenarioDocument *document,
        const Backend::Scenario::ScenarioRegistry *registry,
        int                                        count,
        ::CargoNetSim::CargoNetSimController      *controller);

public slots:
    /// Entry point on the worker thread. Always emits exactly one
    /// of `resultReady()` or `error()`, followed by `finished()`.
    void process();

signals:
    void resultReady(
        const Backend::Scenario::PreparedPathSet &preparedPaths);
    void error(const QString &message);
    void finished();

private:
    Backend::Application::PreparedPathService
    makePreparedPathService() const;

    const Backend::Scenario::ScenarioDocument *m_document = nullptr;
    const Backend::Scenario::ScenarioRegistry *m_registry = nullptr;
    ::CargoNetSim::CargoNetSimController      *m_controller = nullptr;
    int                                        pathsCount = 0;
};

} // namespace GUI
} // namespace CargoNetSim
