#pragma once

#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "SimulationDispatchTypes.h"

namespace CargoNetSim
{
namespace Backend
{
class VehicleController;

namespace Scenario
{

struct SegmentDispatchCursor
{
    int shipCounter = 0;
    int trainCounter = 0;
    int truckCounter = 0;
    int containerCounter = 0;
};

struct TrainSegmentDispatchRequest
{
    int     pathId = -1;
    QString pathIdentity;
    QString canonicalPathKey;
    int     segmentIndex = -1;
    QString startTerminalId;
    QString endTerminalId;
    QString networkName;
    int     runtimeStartNodeId = -1;
    int     runtimeEndNodeId = -1;
    int     trainContainerCapacity = -1;
    QList<ContainerCore::Container *> containersAvailable;
    CargoNetSim::Backend::TrainClient::NeTrainSimNetwork *network =
        nullptr;
};

struct TruckSegmentDispatchRequest
{
    int     pathId = -1;
    QString pathIdentity;
    QString canonicalPathKey;
    int     segmentIndex = -1;
    QString startTerminalId;
    QString endTerminalId;
    QString networkName;
    int     runtimeStartNodeId = -1;
    int     runtimeEndNodeId = -1;
    QString runtimeStartLocationName;
    QString runtimeEndLocationName;
    int     truckContainerCapacity = -1;
    QList<ContainerCore::Container *> containersAvailable;
    CargoNetSim::Backend::TruckClient::IntegrationNetwork *network =
        nullptr;
};

struct ShipSegmentDispatchRequest
{
    int     pathId = -1;
    QString pathIdentity;
    QString canonicalPathKey;
    int     segmentIndex = -1;
    QString startTerminalId;
    QString endTerminalId;
    QString networkName;
    QPointF startGlobalPosition;
    QPointF endGlobalPosition;
    int     shipContainerCapacity = -1;
    QList<ContainerCore::Container *> containersAvailable;
};

class SegmentDispatchFactory
{
public:
    SegmentDispatchFactory(VehicleController *vehicles,
                           const QString     &executionId = QString());

    bool appendTrainSegment(const TrainSegmentDispatchRequest &request,
                            SegmentDispatchCursor            &cursor,
                            SimulationRequestBundle         &bundle,
                            QVector<VehicleLoadManifest>    *vehicleLoads,
                            QString                         *err) const;

    bool appendTruckSegment(const TruckSegmentDispatchRequest &request,
                            SegmentDispatchCursor            &cursor,
                            SimulationRequestBundle         &bundle,
                            QVector<VehicleLoadManifest>    *vehicleLoads,
                            QString                         *err) const;

    bool appendShipSegment(const ShipSegmentDispatchRequest &request,
                           SegmentDispatchCursor           &cursor,
                           SimulationRequestBundle        &bundle,
                           QVector<VehicleLoadManifest>   *vehicleLoads,
                           QString                        *err) const;

private:
    VehicleController *m_vehicles = nullptr;
    QString            m_executionId;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
