#include "ExecutionEventAdapter.h"

#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Commons/LogCategories.h"
#include "RuntimeArtifactIdentity.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QString segmentLookupKey(const QString &canonicalPathKey,
                         int            segmentIndex)
{
    return canonicalPathKey + QStringLiteral("|")
        + QString::number(segmentIndex);
}

bool isSupportedVehicleArtifact(const QString &artifactType)
{
    return artifactType == QStringLiteral("train")
        || artifactType == QStringLiteral("ship")
        || artifactType == QStringLiteral("truck");
}

QString correlationToken(const QString &prefix,
                         const QString &vehicleId)
{
    return prefix + QStringLiteral(":") + vehicleId;
}

} // namespace

ExecutionEventAdapter::ExecutionEventAdapter(QObject *parent)
    : QObject(parent)
{
}

bool ExecutionEventAdapter::initialize(
    const ScenarioExecutionPlan &plan, QString *err)
{
    m_executionId.clear();
    m_executionPathKeyByCanonicalKey.clear();
    m_segmentPlansByLookupKey.clear();

    if (plan.executionId.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                "ExecutionEventAdapter requires a plan with a non-empty execution ID");
        }
        return false;
    }

    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.executionPathKey.isEmpty())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "ExecutionEventAdapter plan contains an empty execution path key");
            }
            return false;
        }
        if (pathPlan.canonicalPathKey.isEmpty())
        {
            if (err)
            {
                *err = QStringLiteral(
                    "ExecutionEventAdapter plan contains an empty canonical path key for %1")
                           .arg(pathPlan.executionPathKey);
            }
            return false;
        }

        const auto existingIdentity =
            m_executionPathKeyByCanonicalKey.constFind(
                pathPlan.canonicalPathKey);
        if (existingIdentity != m_executionPathKeyByCanonicalKey.constEnd()
            && existingIdentity.value() != pathPlan.executionPathKey)
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Canonical path key %1 maps to multiple execution path keys")
                           .arg(pathPlan.canonicalPathKey);
            }
            return false;
        }
        m_executionPathKeyByCanonicalKey.insert(
            pathPlan.canonicalPathKey, pathPlan.executionPathKey);

        for (const auto &segmentPlan : pathPlan.segments)
        {
            if (segmentPlan.executionPathKey != pathPlan.executionPathKey)
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Segment plan %1 does not match parent execution path key %2")
                               .arg(segmentPlan.segmentId,
                                    pathPlan.executionPathKey);
                }
                return false;
            }

            const QString key = segmentLookupKey(
                pathPlan.canonicalPathKey, segmentPlan.segmentIndex);
            if (m_segmentPlansByLookupKey.contains(key))
            {
                if (err)
                {
                    *err = QStringLiteral(
                        "Duplicate segment execution plan mapping for canonical key %1 segment %2")
                               .arg(pathPlan.canonicalPathKey)
                               .arg(segmentPlan.segmentIndex);
                }
                return false;
            }
            m_segmentPlansByLookupKey.insert(key, segmentPlan);
        }
    }

    m_executionId = plan.executionId;
    if (err)
        err->clear();
    return true;
}

void ExecutionEventAdapter::attach(
    TrainClient::TrainSimulationClient  *trainClient,
    ShipClient::ShipSimulationClient    *shipClient,
    TruckClient::TruckSimulationManager *truckManager)
{
    clearConnections();

    if (trainClient)
    {
        m_connections.append(connect(
            trainClient,
            &TrainClient::TrainSimulationClient::segmentVehicleArrived,
            this, &ExecutionEventAdapter::onTrainVehicleArrived,
            Qt::DirectConnection));
        m_connections.append(connect(
            trainClient,
            &TrainClient::TrainSimulationClient::segmentUnloadSucceeded,
            this,
            &ExecutionEventAdapter::onTrainSegmentUnloadSucceeded,
            Qt::DirectConnection));
        m_connections.append(connect(
            trainClient,
            &TrainClient::TrainSimulationClient::segmentUnloadFailed,
            this,
            &ExecutionEventAdapter::onTrainSegmentUnloadFailed,
            Qt::DirectConnection));
        m_connections.append(connect(
            trainClient,
            &TrainClient::TrainSimulationClient::terminalHandoffSucceeded,
            this,
            &ExecutionEventAdapter::onTrainTerminalHandoffSucceeded,
            Qt::DirectConnection));
        m_connections.append(connect(
            trainClient,
            &TrainClient::TrainSimulationClient::terminalHandoffFailed,
            this,
            &ExecutionEventAdapter::onTrainTerminalHandoffFailed,
            Qt::DirectConnection));
    }

    if (shipClient)
    {
        m_connections.append(connect(
            shipClient,
            &ShipClient::ShipSimulationClient::segmentVehicleArrived,
            this, &ExecutionEventAdapter::onShipVehicleArrived,
            Qt::DirectConnection));
        m_connections.append(connect(
            shipClient,
            &ShipClient::ShipSimulationClient::segmentUnloadSucceeded,
            this,
            &ExecutionEventAdapter::onShipSegmentUnloadSucceeded,
            Qt::DirectConnection));
        m_connections.append(connect(
            shipClient,
            &ShipClient::ShipSimulationClient::segmentUnloadFailed,
            this,
            &ExecutionEventAdapter::onShipSegmentUnloadFailed,
            Qt::DirectConnection));
        m_connections.append(connect(
            shipClient,
            &ShipClient::ShipSimulationClient::terminalHandoffSucceeded,
            this,
            &ExecutionEventAdapter::onShipTerminalHandoffSucceeded,
            Qt::DirectConnection));
        m_connections.append(connect(
            shipClient,
            &ShipClient::ShipSimulationClient::terminalHandoffFailed,
            this,
            &ExecutionEventAdapter::onShipTerminalHandoffFailed,
            Qt::DirectConnection));
    }

    if (truckManager)
    {
        m_connections.append(connect(
            truckManager,
            &TruckClient::TruckSimulationManager::tripEndedWithData,
            this, &ExecutionEventAdapter::onTruckTripEndedWithData,
            Qt::DirectConnection));
    }
}

bool ExecutionEventAdapter::buildDispatchEvent(
    const QString &vehicleId, const QString &networkName,
    double eventTimeSeconds, ExecutionEvent *event,
    QString *err) const
{
    if (!event)
    {
        if (err)
            *err = QStringLiteral(
                "ExecutionEventAdapter requires a non-null ExecutionEvent output");
        return false;
    }

    ResolvedExecutionTarget target;
    if (!resolveVehicleTarget(vehicleId, &target, err))
        return false;

    ExecutionEvent dispatchEvent;
    dispatchEvent.type =
        ExecutionEventType::SegmentVehicleDispatched;
    dispatchEvent.executionPathKey = target.executionPathKey;
    dispatchEvent.segmentIndex = target.segmentIndex;
    dispatchEvent.vehicleId    = vehicleId;
    dispatchEvent.networkName =
        networkName.isEmpty() ? target.networkName : networkName;
    dispatchEvent.terminalId = target.startTerminalId;
    dispatchEvent.correlationToken =
        correlationToken(QStringLiteral("dispatch"), vehicleId);
    dispatchEvent.eventTimeSeconds = eventTimeSeconds;

    *event = dispatchEvent;
    if (err)
        err->clear();
    return true;
}

void ExecutionEventAdapter::onTrainVehicleArrived(
    const QString &networkName, const QString &vehicleId,
    const QString &runtimeTerminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitArrivalEvent(target, vehicleId, networkName,
                     runtimeTerminalId, eventTimeSeconds,
                     correlationToken(QStringLiteral("train-arrival"),
                                      vehicleId));
}

void ExecutionEventAdapter::onTrainTerminalHandoffSucceeded(
    const QString &networkName, const QString &vehicleId,
    const QString &scenarioTerminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitTerminalEvent(target, vehicleId, networkName,
                      scenarioTerminalId, eventTimeSeconds,
                      true, QString(),
                      QStringLiteral("train-handoff"));
}

void ExecutionEventAdapter::onTrainSegmentUnloadSucceeded(
    const QString &networkName, const QString &vehicleId,
    const QString &terminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitUnloadEvent(target, vehicleId, networkName, terminalId,
                    eventTimeSeconds,
                    QStringLiteral("train-unload"));
}

void ExecutionEventAdapter::onTrainSegmentUnloadFailed(
    const QString &networkName, const QString &vehicleId,
    const QString &terminalId, const QString &message,
    double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitUnloadFailedEvent(target, vehicleId, networkName,
                          terminalId, message,
                          eventTimeSeconds,
                          QStringLiteral("train-unload-failed"));
}

void ExecutionEventAdapter::onTrainTerminalHandoffFailed(
    const QString &networkName, const QString &vehicleId,
    const QString &message, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitTerminalEvent(target, vehicleId, networkName,
                      target.endTerminalId, eventTimeSeconds,
                      false, message,
                      QStringLiteral("train-handoff"));
}

void ExecutionEventAdapter::onShipVehicleArrived(
    const QString &networkName, const QString &vehicleId,
    const QString &runtimeTerminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitArrivalEvent(target, vehicleId, networkName,
                     runtimeTerminalId, eventTimeSeconds,
                     correlationToken(QStringLiteral("ship-arrival"),
                                      vehicleId));
}

void ExecutionEventAdapter::onShipTerminalHandoffSucceeded(
    const QString &networkName, const QString &vehicleId,
    const QString &scenarioTerminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitTerminalEvent(target, vehicleId, networkName,
                      scenarioTerminalId, eventTimeSeconds,
                      true, QString(),
                      QStringLiteral("ship-handoff"));
}

void ExecutionEventAdapter::onShipSegmentUnloadSucceeded(
    const QString &networkName, const QString &vehicleId,
    const QString &terminalId, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitUnloadEvent(target, vehicleId, networkName, terminalId,
                    eventTimeSeconds,
                    QStringLiteral("ship-unload"));
}

void ExecutionEventAdapter::onShipSegmentUnloadFailed(
    const QString &networkName, const QString &vehicleId,
    const QString &terminalId, const QString &message,
    double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitUnloadFailedEvent(target, vehicleId, networkName,
                          terminalId, message,
                          eventTimeSeconds,
                          QStringLiteral("ship-unload-failed"));
}

void ExecutionEventAdapter::onShipTerminalHandoffFailed(
    const QString &networkName, const QString &vehicleId,
    const QString &message, double eventTimeSeconds)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(vehicleId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitTerminalEvent(target, vehicleId, networkName,
                      target.endTerminalId, eventTimeSeconds,
                      false, message,
                      QStringLiteral("ship-handoff"));
}

void ExecutionEventAdapter::onTruckTripEndedWithData(
    const TruckClient::TripEndData &tripData)
{
    ResolvedExecutionTarget target;
    QString err;
    if (!resolveVehicleTarget(tripData.tripId, &target, &err))
    {
        emitAdapterError(err);
        return;
    }

    emitArrivalEvent(
        target, tripData.tripId, tripData.networkName,
        tripData.destination, -1.0,
        correlationToken(QStringLiteral("truck-arrival"),
                         tripData.tripId));

    emitUnloadEvent(target, tripData.tripId, tripData.networkName,
                    target.endTerminalId, -1.0,
                    QStringLiteral("truck-unload"));
    emitTerminalEvent(target, tripData.tripId,
                      tripData.networkName, target.endTerminalId,
                      -1.0, true, QString(),
                      QStringLiteral("truck-handoff"));
}

bool ExecutionEventAdapter::resolveVehicleTarget(
    const QString &vehicleId, ResolvedExecutionTarget *target,
    QString *err) const
{
    RuntimeArtifactIdentity artifact;
    if (!RuntimeArtifacts::decode(vehicleId, artifact))
    {
        if (err)
        {
            *err = QStringLiteral(
                "ExecutionEventAdapter could not decode runtime artifact ID '%1'")
                       .arg(vehicleId);
        }
        return false;
    }

    if (!isSupportedVehicleArtifact(artifact.artifactType))
    {
        if (err)
        {
            *err = QStringLiteral(
                "ExecutionEventAdapter does not support artifact type '%1' in vehicle ID '%2'")
                       .arg(artifact.artifactType, vehicleId);
        }
        return false;
    }

    return resolveCanonicalSegmentTarget(
        artifact.pathKey, artifact.segmentIndex, target, err);
}

bool ExecutionEventAdapter::resolveCanonicalSegmentTarget(
    const QString &canonicalPathKey, int segmentIndex,
    ResolvedExecutionTarget *target, QString *err) const
{
    if (!target)
    {
        if (err)
        {
            *err = QStringLiteral(
                "ExecutionEventAdapter requires a non-null target output");
        }
        return false;
    }

    const auto executionPathKeyIt =
        m_executionPathKeyByCanonicalKey.constFind(canonicalPathKey);
    if (executionPathKeyIt == m_executionPathKeyByCanonicalKey.constEnd())
    {
        if (err)
        {
            *err = QStringLiteral(
                "Canonical path key '%1' is not present in the execution plan")
                       .arg(canonicalPathKey);
        }
        return false;
    }

    const auto segmentIt = m_segmentPlansByLookupKey.constFind(
        segmentLookupKey(canonicalPathKey, segmentIndex));
    if (segmentIt == m_segmentPlansByLookupKey.constEnd())
    {
        if (err)
        {
            *err = QStringLiteral(
                "Execution plan does not contain segment %1 for canonical path key '%2'")
                       .arg(segmentIndex)
                       .arg(canonicalPathKey);
        }
        return false;
    }

    target->canonicalPathKey = canonicalPathKey;
    target->executionPathKey     = executionPathKeyIt.value();
    target->networkName      = segmentIt.value().networkName;
    target->startTerminalId  = segmentIt.value().startTerminalId;
    target->endTerminalId    = segmentIt.value().endTerminalId;
    target->segmentIndex     = segmentIt.value().segmentIndex;

    if (err)
        err->clear();
    return true;
}

void ExecutionEventAdapter::emitArrivalEvent(
    const ResolvedExecutionTarget &target,
    const QString &vehicleId, const QString &networkName,
    const QString &terminalId, double eventTimeSeconds,
    const QString &token)
{
    ExecutionEvent event;
    event.type = ExecutionEventType::SegmentVehicleArrived;
    event.executionPathKey = target.executionPathKey;
    event.segmentIndex = target.segmentIndex;
    event.vehicleId    = vehicleId;
    event.networkName =
        networkName.isEmpty() ? target.networkName : networkName;
    event.terminalId = terminalId;
    event.correlationToken = token;
    event.eventTimeSeconds = eventTimeSeconds;
    emit executionEventReady(event);
}

void ExecutionEventAdapter::emitUnloadEvent(
    const ResolvedExecutionTarget &target,
    const QString &vehicleId, const QString &networkName,
    const QString &terminalId, double eventTimeSeconds,
    const QString &correlationPrefix)
{
    ExecutionEvent unloadEvent;
    unloadEvent.type =
        ExecutionEventType::SegmentUnloadSucceeded;
    unloadEvent.executionPathKey = target.executionPathKey;
    unloadEvent.segmentIndex = target.segmentIndex;
    unloadEvent.vehicleId    = vehicleId;
    unloadEvent.networkName =
        networkName.isEmpty() ? target.networkName : networkName;
    unloadEvent.terminalId =
        terminalId.isEmpty() ? target.endTerminalId
                             : terminalId;
    unloadEvent.correlationToken =
        correlationToken(correlationPrefix, vehicleId);
    unloadEvent.eventTimeSeconds = eventTimeSeconds;
    emit executionEventReady(unloadEvent);
}

void ExecutionEventAdapter::emitUnloadFailedEvent(
    const ResolvedExecutionTarget &target,
    const QString &vehicleId, const QString &networkName,
    const QString &terminalId, const QString &message,
    double eventTimeSeconds,
    const QString &correlationPrefix)
{
    ExecutionEvent unloadEvent;
    unloadEvent.type =
        ExecutionEventType::SegmentUnloadFailed;
    unloadEvent.executionPathKey = target.executionPathKey;
    unloadEvent.segmentIndex = target.segmentIndex;
    unloadEvent.vehicleId    = vehicleId;
    unloadEvent.networkName =
        networkName.isEmpty() ? target.networkName : networkName;
    unloadEvent.terminalId =
        terminalId.isEmpty() ? target.endTerminalId
                             : terminalId;
    unloadEvent.message = message;
    unloadEvent.correlationToken =
        correlationToken(correlationPrefix, vehicleId);
    unloadEvent.eventTimeSeconds = eventTimeSeconds;
    emit executionEventReady(unloadEvent);
}

void ExecutionEventAdapter::emitTerminalEvent(
    const ResolvedExecutionTarget &target,
    const QString &vehicleId, const QString &networkName,
    const QString &scenarioTerminalId, double eventTimeSeconds,
    bool handoffSucceeded, const QString &message,
    const QString &correlationPrefix)
{
    ExecutionEvent handoffEvent;
    handoffEvent.type = handoffSucceeded
        ? ExecutionEventType::TerminalHandoffSucceeded
        : ExecutionEventType::TerminalHandoffFailed;
    handoffEvent.executionPathKey = target.executionPathKey;
    handoffEvent.segmentIndex = target.segmentIndex;
    handoffEvent.vehicleId    = vehicleId;
    handoffEvent.networkName =
        networkName.isEmpty() ? target.networkName : networkName;
    handoffEvent.terminalId =
        scenarioTerminalId.isEmpty() ? target.endTerminalId
                                     : scenarioTerminalId;
    handoffEvent.message = message;
    handoffEvent.correlationToken =
        correlationToken(correlationPrefix, vehicleId);
    handoffEvent.eventTimeSeconds = eventTimeSeconds;
    emit executionEventReady(handoffEvent);
}

void ExecutionEventAdapter::emitAdapterError(
    const QString &message)
{
    qCWarning(lcScenario)
        << "ExecutionEventAdapter:" << message;
    emit adapterError(message);
}

void ExecutionEventAdapter::clearConnections()
{
    for (const auto &connection : m_connections)
        QObject::disconnect(connection);
    m_connections.clear();
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
