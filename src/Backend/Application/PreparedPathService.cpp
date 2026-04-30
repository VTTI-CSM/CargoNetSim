#include "PreparedPathService.h"

#include "AvailabilityService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioRuntime.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

QString availabilitySummary(
    const QList<BackendAvailabilityStatus> &statuses)
{
    QStringList parts;
    parts.reserve(statuses.size());
    for (const auto &status : statuses)
    {
        parts.append(QStringLiteral(
                         "%1(%2): client=%3 connected=%4 consumers=%5 command=%6")
                         .arg(status.serverKey)
                         .arg(status.server)
                         .arg(status.clientExists)
                         .arg(status.connected)
                         .arg(status.hasConsumers)
                         .arg(status.commandAvailable));
    }
    return parts.join(QStringLiteral("; "));
}

} // namespace

PreparedPathService::PreparedPathService(
    ::CargoNetSim::CargoNetSimController *controller)
    : PreparedPathService(
          controller ? controller->getConfigController() : nullptr,
          controller ? controller->getNetworkController() : nullptr,
          controller ? controller->getRegionDataController() : nullptr)
{
}

PreparedPathService::PreparedPathService(
    ConfigController     *config,
    NetworkController    *networks,
    RegionDataController *regionData)
    : m_config(config)
    , m_networks(networks)
    , m_regionData(regionData)
{
}

PreparedPathServiceResult PreparedPathService::discoverAndPrepare(
    const Scenario::ScenarioDocument &document,
    const Scenario::ScenarioRegistry &registry,
    int                               topN) const
{
    PreparedPathServiceResult result;
    result.topNRequested = topN;

    if (topN <= 0)
    {
        result.status = PreparedPathServiceStatus::InvalidRequest;
        result.message =
            QStringLiteral("Requested path count must be positive");
        return result;
    }

    if (document.originTerminalIds().isEmpty())
    {
        result.status = PreparedPathServiceStatus::NoPathsFound;
        result.message =
            QStringLiteral("No origin/destination pairs in scenario");
        return result;
    }

    AvailabilityService availabilityService;
    const auto availabilityResult =
        availabilityService.waitForCommandAvailability(
            {QStringLiteral("terminal")}, /*timeoutMs=*/10000);
    if (!availabilityResult.satisfied)
    {
        result.status = PreparedPathServiceStatus::BackendUnavailable;
        result.message = QStringLiteral(
            "Required backend command queues are unavailable: %1. "
            "Availability snapshot: %2")
                             .arg(
                                 availabilityResult.missingServerKeys.join(
                                     QStringLiteral(", ")),
                                 availabilitySummary(
                                     availabilityResult.statuses));
        qCWarning(lcScenario)
            << "PreparedPathService::discoverAndPrepare:"
            << result.message;
        return result;
    }

    QString err;
    auto prepared = Scenario::PathPreparationService::discoverAndPreparePaths(
        document, registry, topN, m_config, m_networks, m_regionData,
        &err);

    if (prepared.isEmpty())
    {
        result.status =
            err.isEmpty() ? PreparedPathServiceStatus::NoPathsFound
                          : PreparedPathServiceStatus::DiscoveryFailed;
        result.message =
            err.isEmpty() ? QStringLiteral("No paths found.") : err;
        qCWarning(lcScenario)
            << "PreparedPathService::discoverAndPrepare:"
            << "prepared-path workflow failed -" << result.message;
        return result;
    }

    result.status = PreparedPathServiceStatus::Success;
    result.preparedPathCount = prepared.size();
    result.preparedPaths = std::move(prepared);
    qCInfo(lcScenario)
        << "PreparedPathService::discoverAndPrepare:"
        << "prepared" << result.preparedPathCount << "path(s)";
    return result;
}

PreparedPathServiceResult PreparedPathService::discoverAndPrepare(
    const Scenario::ScenarioRuntime &runtime,
    int                              topN) const
{
    return discoverAndPrepare(runtime.document(), runtime.registry(), topN);
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
