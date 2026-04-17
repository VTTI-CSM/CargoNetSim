#include "NetworkLookup.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/ScenarioRegistry.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace NetworkLookup
{

QMap<QString, TrainClient::NeTrainSimNetwork *>
collectRail(const ScenarioRegistry &registry,
            const QString          &regionName)
{
    qCDebug(lcScenario) << "NetworkLookup::collectRail:"
                        << "region =" << regionName;

    QMap<QString, TrainClient::NeTrainSimNetwork *> result;
    if (registry.hasPreviewNetworks())
    {
        for (const QString &n : registry.previewRailNetworkNames())
            if (auto *net = registry.previewRailNetwork(n))
                result.insert(n, net);
        qCDebug(lcScenario) << "NetworkLookup::collectRail:"
                            << "found" << result.size()
                            << "preview rail networks";
        return result;
    }
    auto *rdc =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController();
    auto *rd = rdc ? rdc->getRegionData(regionName) : nullptr;
    if (!rd)
    {
        qCWarning(lcScenario) << "NetworkLookup::collectRail:"
                              << "no region data for" << regionName;
        return result;
    }
    for (const QString &n : rd->getTrainNetworks())
        if (auto *net = rd->getTrainNetwork(n))
            result.insert(n, net);
    qCDebug(lcScenario) << "NetworkLookup::collectRail:"
                        << "found" << result.size()
                        << "rail networks in region" << regionName;
    return result;
}

QMap<QString, TruckClient::IntegrationNetwork *>
collectTruck(const ScenarioRegistry &registry,
             const QString          &regionName)
{
    qCDebug(lcScenario) << "NetworkLookup::collectTruck:"
                        << "region =" << regionName;

    QMap<QString, TruckClient::IntegrationNetwork *> result;
    if (registry.hasPreviewNetworks())
    {
        for (const QString &n : registry.previewTruckNetworkNames())
            if (auto *net = registry.previewTruckNetwork(n))
                result.insert(n, net);
        qCDebug(lcScenario) << "NetworkLookup::collectTruck:"
                            << "found" << result.size()
                            << "preview truck networks";
        return result;
    }
    auto *rdc =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController();
    auto *rd = rdc ? rdc->getRegionData(regionName) : nullptr;
    if (!rd)
    {
        qCWarning(lcScenario) << "NetworkLookup::collectTruck:"
                              << "no region data for" << regionName;
        return result;
    }
    for (const QString &n : rd->getTruckNetworks())
        if (auto *net = rd->getTruckNetwork(n))
            result.insert(n, net);
    qCDebug(lcScenario) << "NetworkLookup::collectTruck:"
                        << "found" << result.size()
                        << "truck networks in region" << regionName;
    return result;
}

TrainClient::NeTrainSimNetwork *
findRail(const ScenarioRegistry &registry,
         const QString          &regionName,
         const QString          &networkName)
{
    qCDebug(lcScenario) << "NetworkLookup::findRail:"
                        << "region =" << regionName
                        << ", network =" << networkName;

    auto *net = collectRail(registry, regionName).value(networkName, nullptr);
    if (!net)
        qCWarning(lcScenario) << "NetworkLookup::findRail:"
                              << "rail network" << networkName
                              << "not found in region" << regionName;
    return net;
}

TruckClient::IntegrationNetwork *
findTruck(const ScenarioRegistry &registry,
          const QString          &regionName,
          const QString          &networkName)
{
    qCDebug(lcScenario) << "NetworkLookup::findTruck:"
                        << "region =" << regionName
                        << ", network =" << networkName;

    auto *net = collectTruck(registry, regionName).value(networkName, nullptr);
    if (!net)
        qCWarning(lcScenario) << "NetworkLookup::findTruck:"
                              << "truck network" << networkName
                              << "not found in region" << regionName;
    return net;
}

QString networkNameOf(QObject *network, NetworkKind *outKind)
{
    if (!network) return QString();
    if (auto *rail = qobject_cast<TrainClient::NeTrainSimNetwork *>(network))
    {
        if (outKind) *outKind = NetworkKind::Rail;
        const QString name = rail->getNetworkName();
        qCDebug(lcScenario) << "NetworkLookup::networkNameOf:"
                            << "resolved rail network ->" << name;
        return name;
    }
    if (auto *truck = qobject_cast<TruckClient::IntegrationNetwork *>(network))
    {
        if (outKind) *outKind = NetworkKind::Truck;
        const QString name = truck->getNetworkName();
        qCDebug(lcScenario) << "NetworkLookup::networkNameOf:"
                            << "resolved truck network ->" << name;
        return name;
    }
    qCWarning(lcScenario) << "NetworkLookup::networkNameOf:"
                          << "unknown network type, could not resolve name";
    return QString();
}

} // namespace NetworkLookup
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
