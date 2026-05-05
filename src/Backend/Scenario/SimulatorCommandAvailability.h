#pragma once

#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

inline bool isCommandAvailable(bool connected, bool hasConsumers)
{
    return connected && hasConsumers;
}

inline bool isCommandAvailable(
    CargoNetSim::Backend::SimulationClientBase *client)
{
    if (!client)
        return false;

    return client->probeCommandAvailability();
}

inline bool isCommandAvailable(
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *manager)
{
    if (!manager)
        return false;

    if (manager->getAllClientNames().isEmpty())
        return false;

    return isCommandAvailable(manager->isConnected(),
                              manager->hasCommandQueueConsumers());
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
