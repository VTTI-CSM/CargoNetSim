#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

struct SimulatorAvailability
{
    bool terminalAvailable = false;
    bool trainAvailable    = false;
    bool shipAvailable     = false;
    bool truckAvailable    = false;
};

struct PreparedPathRequirements
{
    bool terminalRequired = true;
    bool trainRequired    = false;
    bool shipRequired     = false;
    bool truckRequired    = false;
};

struct PreparedPathEligibility
{
    bool    selectable  = true;
    bool    simulatable = true;
    QString blockingReason;
    QString warningReason;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
