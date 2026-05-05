#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "LinkageSource.h"
#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Region-local connection between two terminals via a specific transport mode.
struct Connection
{
    QString                                 fromTerminalId;
    QString                                 toTerminalId;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Truck;
    QString                 region;  ///< must match both endpoints
    QMap<QString, QVariant> properties;
    LinkageSource source = LinkageSource::Manual;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
