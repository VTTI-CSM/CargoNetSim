#include "GUI/Utils/PathFindingWorker.h"

#include <QMetaType>

#include "Backend/Commons/LogCategories.h"

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
    Backend::ConfigController                 *config,
    Backend::NetworkController                *networks,
    Backend::RegionDataController             *regionData,
    Backend::VehicleController                *vehicles)
{
    qCDebug(lcGuiUtil) << "PathFindingWorker::initialize: pathsCount" << count;
    m_document   = document;
    m_registry   = registry;
    m_config     = config;
    m_networks   = networks;
    m_regionData = regionData;
    m_vehicles   = vehicles;
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

    QString err;
    auto prepared =
        Backend::Scenario::PathPreparationService::
            discoverAndPreparePaths(
                *m_document, *m_registry, pathsCount,
                m_config, m_networks, m_regionData,
                m_vehicles, &err);

    if (prepared.isEmpty())
    {
        qCWarning(lcGuiUtil) << "PathFindingWorker::process: no paths found -"
                             << (err.isEmpty() ? "no details" : err);
        emit error(err.isEmpty() ? QStringLiteral("No paths found.")
                                 : err);
        emit finished();
        return;
    }

    qCInfo(lcGuiUtil) << "PathFindingWorker::process: prepared"
                      << prepared.size() << "paths";
    emit resultReady(prepared);
    emit finished();
}

} // namespace GUI
} // namespace CargoNetSim
