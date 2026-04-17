#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace CargoNetSim {
namespace Backend {
class Path;
} // namespace Backend

namespace GUI {

class MainWindow;

/**
 * @brief Worker-thread wrapper that drives
 *        `Backend::Scenario::PathDiscovery` off the GUI thread.
 *
 * Plan 5 Task 5 reduced this class to a minimal QObject thread
 * boundary. All path-finding logic lives in
 * `Backend::Scenario::PathDiscovery` — this worker only:
 *   * reads the scenario from `MainWindow::runtime()`,
 *   * delegates to `PathDiscovery::findTopPaths`,
 *   * re-emits the result (or the error) via Qt signals so the
 *     GUI can display them without blocking the event loop.
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

    /// Bind the worker to a MainWindow (for its ScenarioRuntime)
    /// and the top-N count. Must be called before `process()`.
    void initialize(MainWindow *window, int count);

public slots:
    /// Entry point on the worker thread. Always emits exactly one
    /// of `resultReady()` or `error()`, followed by `finished()`.
    void process();

signals:
    void resultReady(const QList<Backend::Path *> &paths);
    void error(const QString &message);
    void finished();

private:
    MainWindow *mainWindow = nullptr;
    int         pathsCount = 0;
};

} // namespace GUI
} // namespace CargoNetSim
