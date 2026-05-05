#pragma once

#include "LinkageSource.h"
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Binds a terminal to a specific node inside a network.
struct NodeLinkage
{
    QString terminalId;
    QString networkName;
    int     nodeId   = -1;
    LinkageSource source = LinkageSource::Manual;
    bool    excluded = false;  ///< In hybrid strategy: drop even if auto would add it.
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
