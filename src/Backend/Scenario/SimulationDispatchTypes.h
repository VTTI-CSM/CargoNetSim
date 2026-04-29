#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/TrainSystem.h"

#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

struct ShipSimData
{
    CargoNetSim::Backend::Ship       *ship = nullptr;
    QList<ContainerCore::Container *> containers;
    QString                           destinationTerminal;
};

struct TrainSimData
{
    CargoNetSim::Backend::Train      *train = nullptr;
    QList<ContainerCore::Container *> containers;
};

struct TruckSimData
{
    QString                           tripId;
    int                               originNode      = -1;
    int                               destinationNode = -1;
    QList<ContainerCore::Container *> containers;
};

struct VehicleLoadManifest
{
    QString vehicleId;
    QString networkName;
    QStringList logicalContainerIds;
};

struct VehicleDispatchAssignment
{
    QString pathIdentity;
    QString canonicalPathKey;
    int     pathId = -1;
    int     segmentIndex = -1;
    QString vehicleId;
    QString networkName;
    QString startTerminalId;
    QString endTerminalId;
    QStringList logicalContainerIds;
};

struct SimulationRequestBundle
{
    QMap<QString, QList<ShipSimData>>  shipData;
    QMap<QString, QList<TrainSimData>> trainData;
    QMap<QString, QList<TruckSimData>> truckData;
    QMap<QString, CargoNetSim::Backend::TrainClient::NeTrainSimNetwork *>
        trainNetworks;
    QMap<QString, CargoNetSim::Backend::TruckClient::IntegrationNetwork *>
        truckNetworks;

    void merge(const SimulationRequestBundle &other);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
