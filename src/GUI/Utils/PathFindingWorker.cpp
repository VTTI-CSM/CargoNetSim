#include "GUI/Utils/PathFindingWorker.h"
#include "Backend/Commons/LogCategories.h"

#include "Backend/Models/Path.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"

namespace CargoNetSim {
namespace GUI {

PathFindingWorker::PathFindingWorker()
    : QObject(nullptr)
{
    qCDebug(lcGuiUtil) << "PathFindingWorker::PathFindingWorker: created";
}

void PathFindingWorker::initialize(MainWindow *window, int count)
{
    qCDebug(lcGuiUtil) << "PathFindingWorker::initialize: pathsCount" << count;
    mainWindow = window;
    pathsCount = count;
}

void PathFindingWorker::process()
{
    qCInfo(lcGuiUtil) << "PathFindingWorker::process: starting path discovery for"
                      << pathsCount << "paths";
    // Task 5 (Plan 5) rewire: all path-finding logic now lives in
    // Backend::Scenario::PathDiscovery, which walks the document
    // directly and uses the registry-populated Backend::Terminal*s
    // from Plan 2's applier. This worker is responsible only for
    // the thread boundary + Qt signal protocol.
    if (!mainWindow || !mainWindow->runtime())
    {
        qCWarning(lcGuiUtil) << "PathFindingWorker::process: no scenario loaded";
        emit error(QStringLiteral("No scenario loaded."));
        emit finished();
        return;
    }

    auto                           &rt = *mainWindow->runtime();
    Backend::Scenario::PathDiscovery pd;
    QString                          err;
    auto paths = pd.findTopPaths(rt.document(), rt.registry(),
                                 pathsCount, &err);

    if (paths.isEmpty())
    {
        qCWarning(lcGuiUtil) << "PathFindingWorker::process: no paths found -"
                             << (err.isEmpty() ? "no details" : err);
        emit error(err.isEmpty() ? QStringLiteral("No paths found.")
                                 : err);
        emit finished();
        return;
    }

    qCInfo(lcGuiUtil) << "PathFindingWorker::process: found" << paths.size() << "paths";
    emit resultReady(paths);
    emit finished();
}

} // namespace GUI
} // namespace CargoNetSim
