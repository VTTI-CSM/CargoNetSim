#pragma once

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>

#include "Backend/Clients/TruckClient/TripEndCallback.h"
#include "ExecutionPlanTypes.h"

namespace CargoNetSim
{
namespace Backend
{
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

class ExecutionEventAdapter : public QObject
{
    Q_OBJECT

public:
    explicit ExecutionEventAdapter(QObject *parent = nullptr);

    bool initialize(const ScenarioExecutionPlan &plan,
                    QString                     *err = nullptr);

    void attach(TrainClient::TrainSimulationClient  *trainClient,
                ShipClient::ShipSimulationClient    *shipClient,
                TruckClient::TruckSimulationManager *truckManager);

    bool buildDispatchEvent(const QString &vehicleId,
                            const QString &networkName,
                            double         eventTimeSeconds,
                            ExecutionEvent *event,
                            QString        *err = nullptr) const;

public slots:
    void onTrainVehicleArrived(const QString &networkName,
                               const QString &vehicleId,
                               const QString &runtimeTerminalId,
                               double         eventTimeSeconds);
    void onTrainSegmentUnloadSucceeded(
        const QString &networkName,
        const QString &vehicleId,
        const QString &terminalId,
        double         eventTimeSeconds);
    void onTrainSegmentUnloadFailed(
        const QString &networkName,
        const QString &vehicleId,
        const QString &terminalId,
        const QString &message,
        double         eventTimeSeconds);
    void onTrainTerminalHandoffSucceeded(
        const QString &networkName,
        const QString &vehicleId,
        const QString &scenarioTerminalId,
        double         eventTimeSeconds);
    void onTrainTerminalHandoffFailed(
        const QString &networkName,
        const QString &vehicleId,
        const QString &message,
        double         eventTimeSeconds);

    void onShipVehicleArrived(const QString &networkName,
                              const QString &vehicleId,
                              const QString &runtimeTerminalId,
                              double         eventTimeSeconds);
    void onShipSegmentUnloadSucceeded(
        const QString &networkName,
        const QString &vehicleId,
        const QString &terminalId,
        double         eventTimeSeconds);
    void onShipSegmentUnloadFailed(
        const QString &networkName,
        const QString &vehicleId,
        const QString &terminalId,
        const QString &message,
        double         eventTimeSeconds);
    void onShipTerminalHandoffSucceeded(
        const QString &networkName,
        const QString &vehicleId,
        const QString &scenarioTerminalId,
        double         eventTimeSeconds);
    void onShipTerminalHandoffFailed(
        const QString &networkName,
        const QString &vehicleId,
        const QString &message,
        double         eventTimeSeconds);

    void onTruckTripEndedWithData(
        const TruckClient::TripEndData &tripData);

signals:
    void executionEventReady(
        const CargoNetSim::Backend::Scenario::ExecutionEvent &event);
    void adapterError(const QString &message);

private:
    struct ResolvedExecutionTarget
    {
        QString canonicalPathKey;
        QString pathIdentity;
        QString networkName;
        QString startTerminalId;
        QString endTerminalId;
        int     segmentIndex = -1;
    };

    bool resolveVehicleTarget(const QString         &vehicleId,
                              ResolvedExecutionTarget *target,
                              QString               *err) const;
    bool resolveCanonicalSegmentTarget(
        const QString            &canonicalPathKey,
        int                       segmentIndex,
        ResolvedExecutionTarget  *target,
        QString                  *err) const;

    void emitArrivalEvent(const ResolvedExecutionTarget &target,
                          const QString                 &vehicleId,
                          const QString                 &networkName,
                          const QString                 &terminalId,
                          double                         eventTimeSeconds,
                          const QString                 &correlationToken);
    void emitUnloadEvent(
        const ResolvedExecutionTarget &target,
        const QString                 &vehicleId,
        const QString                 &networkName,
        const QString                 &terminalId,
        double                         eventTimeSeconds,
        const QString                 &correlationPrefix);
    void emitUnloadFailedEvent(
        const ResolvedExecutionTarget &target,
        const QString                 &vehicleId,
        const QString                 &networkName,
        const QString                 &terminalId,
        const QString                 &message,
        double                         eventTimeSeconds,
        const QString                 &correlationPrefix);
    void emitTerminalEvent(
        const ResolvedExecutionTarget &target,
        const QString                 &vehicleId,
        const QString                 &networkName,
        const QString                 &scenarioTerminalId,
        double                         eventTimeSeconds,
        bool                           handoffSucceeded,
        const QString                 &message,
        const QString                 &correlationPrefix);
    void emitAdapterError(const QString &message);
    void clearConnections();

    QString m_executionId;
    QHash<QString, QString> m_pathIdentityByCanonicalKey;
    QHash<QString, SegmentExecutionPlan> m_segmentPlansByLookupKey;
    QVector<QMetaObject::Connection> m_connections;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
