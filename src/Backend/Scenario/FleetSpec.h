// src/Backend/Scenario/FleetSpec.h
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

/// Fleet definition sources. For each mode the scenario may point at one or
/// more files and/or supply inline entries. All sources are merged at apply
/// time.
///
/// File formats (existing loaders):
///   - trainsFiles → parsed by Backend::TrainsReader::readTrainsFile
///   - shipsFiles  → parsed by Backend::ShipsReader::readShipsFile
///   - trucksFiles → no equivalent file loader; forwarded to TruckClient via
///                   TruckFleetSpec.
struct FleetSpec
{
    QStringList        trainsFiles;
    QStringList        shipsFiles;
    QStringList        trucksFiles;
    QList<QJsonObject> trainsInline;
    QList<QJsonObject> shipsInline;
    QList<QJsonObject> trucksInline;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
