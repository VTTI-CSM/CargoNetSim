#pragma once

#include <QString>

#include "Backend/Scenario/PathPreparationService.h"

namespace CargoNetSim
{
class CargoNetSimController;

namespace Backend
{
class ConfigController;
class NetworkController;
class RegionDataController;
class VehicleController;

namespace Scenario
{
class ScenarioDocument;
class ScenarioRegistry;
class ScenarioRuntime;
}

namespace Application
{

enum class PreparedPathServiceStatus
{
    Success,
    InvalidRequest,
    NoPathsFound,
    BackendUnavailable,
    DiscoveryFailed
};

struct PreparedPathServiceResult
{
    PreparedPathServiceStatus status =
        PreparedPathServiceStatus::DiscoveryFailed;
    QString                   message;
    int                       topNRequested = 0;
    int                       preparedPathCount = 0;
    Scenario::PreparedPathSet preparedPaths;

    bool succeeded() const
    {
        return status == PreparedPathServiceStatus::Success;
    }
};

class PreparedPathService
{
public:
    explicit PreparedPathService(::CargoNetSim::CargoNetSimController *controller);
    PreparedPathService(ConfigController     *config,
                        NetworkController    *networks,
                        RegionDataController *regionData,
                        VehicleController    *vehicles);

    PreparedPathServiceResult discoverAndPrepare(
        const Scenario::ScenarioDocument &document,
        const Scenario::ScenarioRegistry &registry,
        int                               topN) const;

    PreparedPathServiceResult discoverAndPrepare(
        const Scenario::ScenarioRuntime &runtime,
        int                              topN) const;

private:
    ConfigController     *m_config = nullptr;
    NetworkController    *m_networks = nullptr;
    RegionDataController *m_regionData = nullptr;
    VehicleController    *m_vehicles = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
