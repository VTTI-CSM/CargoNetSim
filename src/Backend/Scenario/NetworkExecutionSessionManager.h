#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include "Backend/Commons/TransportationMode.h"
#include "ExecutionPlanTypes.h"
#include "SimulationDispatchTypes.h"

namespace CargoNetSim
{
namespace Backend
{
class RegionDataController;
namespace ShipClient
{
class ShipSimulationClient;
}
namespace TrainClient
{
class TrainSimulationClient;
}
namespace TruckClient
{
class TruckSimulationManager;
}
namespace Scenario
{

class ScenarioRegistry;

class NetworkExecutionSessionManager
{
public:
    NetworkExecutionSessionManager(
        const ScenarioRegistry                    &registry,
        RegionDataController                      *regionDataController,
        TrainClient::TrainSimulationClient        *trainClient,
        ShipClient::ShipSimulationClient          *shipClient,
        TruckClient::TruckSimulationManager       *truckManager,
        const QString                             &truckExecutablePath = QString());

    void clear();

    bool dispatchWave(const ScenarioExecutionPlan            &plan,
                      const SimulationRequestBundle          &bundle,
                      const QVector<VehicleDispatchAssignment> &assignments,
                      QString                               *err = nullptr);

    bool registerAssignments(
        const ScenarioExecutionPlan              &plan,
        const QVector<VehicleDispatchAssignment> &assignments,
        QString                                  *err = nullptr);

    bool advanceActiveSessions(double   deltaTSeconds,
                               QString *err = nullptr);

    void observeExecutionEvent(const ExecutionEvent &event);

    bool hasActiveVehicles() const;

    QHash<QString, NetworkExecutionSessionState> sessionStates() const;

private:
    struct SessionRecord
    {
        NetworkExecutionSessionState state;
        QString regionName;
        QSet<QString> activeSimulatorVehicleIds;
        QSet<QString> completedSimulatorVehicleIds;
        QHash<QString, int> activeSimulatorVehicleCountsBySegmentKey;
        QHash<QString, QString> segmentKeyByVehicleId;
    };

    QString sessionKey(
        TransportationTypes::TransportationMode mode,
        const QString                          &networkName) const;
    QString segmentKey(const QString &executionPathKey,
                       int            segmentIndex) const;

    const SegmentExecutionPlan *findSegmentPlan(
        const ScenarioExecutionPlan    &plan,
        const VehicleDispatchAssignment &assignment) const;

    bool ensureTrainModeInitialized(QString *err);
    bool ensureShipModeInitialized(QString *err);
    bool ensureTruckModeInitialized(QString *err);

    bool dispatchTrainWave(const SimulationRequestBundle &bundle,
                           QString                      *err);
    bool dispatchShipWave(const SimulationRequestBundle &bundle,
                          QString                      *err);
    bool dispatchTruckWave(
        const QHash<QString, QString> &regionByTruckNetwork,
        const SimulationRequestBundle &bundle,
        QString                      *err);

    const ScenarioRegistry              &m_registry;
    RegionDataController                *m_regionDataController = nullptr;
    TrainClient::TrainSimulationClient  *m_trainClient = nullptr;
    ShipClient::ShipSimulationClient    *m_shipClient = nullptr;
    TruckClient::TruckSimulationManager *m_truckManager = nullptr;
    QString                              m_truckExecutablePath;
    bool                                 m_trainModeInitialized = false;
    bool                                 m_shipModeInitialized = false;
    bool                                 m_truckModeInitialized = false;
    QHash<QString, SessionRecord>        m_sessions;
    QHash<QString, QString>              m_sessionKeyByVehicleId;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
