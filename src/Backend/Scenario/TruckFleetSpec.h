// src/Backend/Scenario/TruckFleetSpec.h
#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Truck-fleet spec carried from the scenario to the executor.
struct TruckFleetSpec
{
    QStringList        files;
    QList<QJsonObject> inline_;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
