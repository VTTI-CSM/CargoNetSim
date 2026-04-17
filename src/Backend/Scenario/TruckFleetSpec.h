#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Truck-fleet spec carried from the scenario to Plan 3's executor.
/// VehicleController has no truck API today; Plan 3 feeds this into
/// TruckClient::ClientConfiguration::masterFilePath (for file-based) and
/// inline entries when the scenario provides them.
struct TruckFleetSpec
{
    QString            file;
    QList<QJsonObject> inline_;  // "inline" is a reserved word.
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
