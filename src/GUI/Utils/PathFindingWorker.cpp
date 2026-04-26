#include "GUI/Utils/PathFindingWorker.h"

#include <QMetaType>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"

namespace CargoNetSim {
namespace GUI {

PathFindingWorker::PathFindingWorker()
    : QObject(nullptr)
{
    qRegisterMetaType<Backend::Scenario::PreparedPathSet>();
    qCDebug(lcGuiUtil) << "PathFindingWorker::PathFindingWorker: created";
}

void PathFindingWorker::initialize(
    const Backend::Scenario::ScenarioDocument *document,
    const Backend::Scenario::ScenarioRegistry *registry,
    int                                        count,
    ::CargoNetSim::CargoNetSimController      *controller)
{
    qCDebug(lcGuiUtil) << "PathFindingWorker::initialize: pathsCount" << count;
    m_document   = document;
    m_registry   = registry;
    m_controller = controller;
    pathsCount   = count;
}

void PathFindingWorker::process()
{
    qCInfo(lcGuiUtil) << "PathFindingWorker::process: starting path discovery for"
                      << pathsCount << "paths";
    if (!m_document || !m_registry)
    {
        qCWarning(lcGuiUtil) << "PathFindingWorker::process: no scenario loaded";
        emit error(QStringLiteral("No scenario loaded."));
        emit finished();
        return;
    }

    const auto result =
        makePreparedPathService().discoverAndPrepare(
            *m_document, *m_registry, pathsCount);

    if (!result.succeeded())
    {
        qCWarning(lcGuiUtil) << "PathFindingWorker::process: no paths found -"
                             << (result.message.isEmpty() ? "no details"
                                                          : result.message);
        emit error(result.message.isEmpty()
                       ? QStringLiteral("No paths found.")
                       : result.message);
        emit finished();
        return;
    }

    qCInfo(lcGuiUtil) << "PathFindingWorker::process: prepared"
                      << result.preparedPathCount << "paths";
    emit resultReady(result.preparedPaths);
    emit finished();
}

Backend::Application::PreparedPathService
PathFindingWorker::makePreparedPathService() const
{
    return Backend::Application::PreparedPathService(m_controller);
}

} // namespace GUI
} // namespace CargoNetSim
