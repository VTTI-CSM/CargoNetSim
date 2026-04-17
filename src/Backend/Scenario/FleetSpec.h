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

/// Fleet definition sources. For each mode the scenario may point at a file
/// and/or supply inline entries. Both sources are merged at apply time (Plan 2).
///
/// File formats (existing loaders — NOT JSON, kept for parity with the GUI):
///   - trainsFile → parsed by Backend::TrainsReader::readTrainsFile
///                  (src/Backend/Models/TrainSystem.cpp)
///   - shipsFile  → parsed by Backend::ShipsReader::readShipsFile
///                  (src/Backend/Models/ShipSystem.cpp, line 1490)
///   - trucksFile → no equivalent file loader exists today; trucks are
///                  currently declared only inline or injected via
///                  TruckClient::TruckSimulationManager. Plan 2's Applier will
///                  decide: drop the field, or define a new reader.
///
/// Inline entries are JSON objects forwarded to the existing JSON
/// constructors (there are no static `fromJson` factories):
///   - trainsInline[i] → `Backend::Train(const QJsonObject&, QObject*)`
///                       at src/Backend/Models/TrainSystem.h:493
///   - shipsInline[i]  → `Backend::Ship(const QJsonObject&, QObject*)`
///                       at src/Backend/Models/ShipSystem.h:172
///   - trucksInline[i] → no Backend::Truck model class exists; Plan 2's
///                       Applier will translate these into the JSON payload
///                       expected by TruckClient (see
///                       Backend/Clients/TruckClient/TruckState.h:31 for the
///                       runtime-state JSON schema used today).
struct FleetSpec
{
    QString trainsFile;
    QString shipsFile;
    QString trucksFile;
    QList<QJsonObject> trainsInline;
    QList<QJsonObject> shipsInline;
    QList<QJsonObject> trucksInline;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
