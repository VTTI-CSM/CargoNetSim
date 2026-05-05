#include "SimulationDispatchTypes.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

void SimulationRequestBundle::merge(const SimulationRequestBundle &other)
{
    for (auto it = other.shipData.constBegin();
         it != other.shipData.constEnd(); ++it)
    {
        shipData[it.key()].append(it.value());
    }

    for (auto it = other.trainData.constBegin();
         it != other.trainData.constEnd(); ++it)
    {
        trainData[it.key()].append(it.value());
    }

    for (auto it = other.truckData.constBegin();
         it != other.truckData.constEnd(); ++it)
    {
        truckData[it.key()].append(it.value());
    }

    for (auto it = other.trainNetworks.constBegin();
         it != other.trainNetworks.constEnd(); ++it)
    {
        trainNetworks.insert(it.key(), it.value());
    }

    for (auto it = other.truckNetworks.constBegin();
         it != other.truckNetworks.constEnd(); ++it)
    {
        truckNetworks.insert(it.key(), it.value());
    }
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
