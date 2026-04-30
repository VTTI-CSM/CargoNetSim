#include "TerminalPickupCoordinator.h"

#include "ExecutionContainerIdentity.h"
#include "TerminalInventoryGateway.h"

#include <containerLib/container.h>

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QString reservationIdFor(const TerminalPickupRequest &request)
{
    const QByteArray material =
        request.executionId.toUtf8() + '\x1f'
        + request.pathIdentity.toUtf8() + '\x1f'
        + request.canonicalPathKey.toUtf8() + '\x1f'
        + request.terminalId.toUtf8() + '\x1f'
        + QByteArray::number(request.segmentIndex);
    const QByteArray digest =
        QCryptographicHash::hash(material,
                                 QCryptographicHash::Sha256)
            .toHex();
    return QStringLiteral("tpr|v1|sig=%1")
        .arg(QString::fromLatin1(digest));
}

void deleteSeededContainers(
    QHash<QString, QList<ContainerCore::Container *>> &containersByTerminal)
{
    for (auto it = containersByTerminal.begin();
         it != containersByTerminal.end(); ++it)
    {
        qDeleteAll(it.value());
        it.value().clear();
    }
}

QList<ContainerCore::Container *> containersFromResponse(
    const QJsonObject &response)
{
    QList<ContainerCore::Container *> containers;
    const QJsonArray array =
        response.value(QStringLiteral("containers")).toArray();
    containers.reserve(array.size());
    for (const auto &value : array)
    {
        const QJsonObject object = value.toObject();
        if (!object.isEmpty())
            containers.append(new ContainerCore::Container(object));
    }
    return containers;
}

bool responseStateIs(const QJsonObject &response,
                     const QString     &expected)
{
    return response.value(QStringLiteral("state")).toString()
        == expected;
}

} // namespace

TerminalPickupCoordinator::TerminalPickupCoordinator(
    TerminalInventoryGateway *gateway)
    : m_gateway(gateway)
{
}

bool TerminalPickupCoordinator::seedExecutionInventory(
    const ScenarioExecutionPlan &plan,
    const PathAllocation        &allocation,
    double                       addTimeSeconds,
    QString                     *err) const
{
    if (!m_gateway)
    {
        if (err)
        {
            *err = QStringLiteral(
                "Terminal pickup coordinator requires a terminal inventory gateway");
        }
        return false;
    }

    QHash<QString, QList<ContainerCore::Container *>>
        containersByTerminal;

    for (const auto &pathPlan : plan.paths)
    {
        if (pathPlan.disposition != PlannedPathDisposition::Execute)
            continue;

        const auto allocatedContainers =
            allocation.byCanonicalPath.value(
                pathPlan.canonicalPathKey);
        if (allocatedContainers.size()
            != pathPlan.effectiveContainerCount)
        {
            deleteSeededContainers(containersByTerminal);
            if (err)
            {
                *err = QStringLiteral(
                    "Terminal inventory seed count mismatch for %1: expected %2, got %3")
                           .arg(pathPlan.pathIdentity)
                           .arg(pathPlan.effectiveContainerCount)
                           .arg(allocatedContainers.size());
            }
            return false;
        }

        for (const auto *container : allocatedContainers)
        {
            if (!container)
                continue;

            const auto metadata =
                ExecutionContainers::makeIdentityMetadata(
                    plan.executionId,
                    pathPlan.pathIdentity,
                    pathPlan.canonicalPathKey,
                    *container,
                    /*readySegmentIndex=*/0,
                    /*terminalSequenceIndex=*/0);
            containersByTerminal[pathPlan.originId].append(
                ExecutionContainers::makeExecutionContainerCopy(
                    *container, metadata, pathPlan.originId));
        }
    }

    bool ok = true;
    for (auto it = containersByTerminal.begin();
         it != containersByTerminal.end(); ++it)
    {
        if (it.value().isEmpty())
            continue;

        if (!m_gateway->addContainers(
                it.key(),
                it.value(),
                addTimeSeconds,
                QString(),
                TerminalInventoryArrivalSemantics::Preload))
        {
            ok = false;
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to seed %1 execution container(s) into terminal %2")
                           .arg(it.value().size())
                           .arg(it.key());
            }
            break;
        }
    }

    deleteSeededContainers(containersByTerminal);
    if (ok && err)
        err->clear();
    return ok;
}

TerminalPickupBatch TerminalPickupCoordinator::reserveForDispatch(
    const TerminalPickupRequest &request) const
{
    TerminalPickupBatch batch;

    if (!m_gateway)
    {
        batch.errorMessage = QStringLiteral(
            "Terminal pickup coordinator requires a terminal inventory gateway");
        return batch;
    }
    if (request.executionId.isEmpty()
        || request.pathIdentity.isEmpty()
        || request.terminalId.isEmpty()
        || request.segmentIndex < 0
        || request.containerCount <= 0)
    {
        batch.errorMessage = QStringLiteral(
            "Terminal pickup reservation request is incomplete");
        return batch;
    }

    batch.handle = makeHandle(request);
    const QJsonObject criteria =
        ExecutionContainers::terminalPickupCriteria(
            request.executionId,
            request.pathIdentity,
            request.segmentIndex,
            request.canonicalPathKey,
            request.containerCount);

    const QJsonObject response = m_gateway->reserveContainers(
        request.terminalId,
        batch.handle.reservationId,
        criteria);
    if (response.isEmpty())
    {
        batch.errorMessage = QStringLiteral(
            "Terminal %1 did not create reservation %2")
                                 .arg(request.terminalId,
                                      batch.handle.reservationId);
        return batch;
    }
    if (responseStateIs(response, QStringLiteral("blocked")))
    {
        batch.blocked = true;
        return batch;
    }
    if (!responseStateIs(response, QStringLiteral("active")))
    {
        batch.errorMessage = QStringLiteral(
            "Terminal reservation %1 for %2 segment %3 is not active (state=%4)")
                                 .arg(batch.handle.reservationId,
                                      request.pathIdentity)
                                 .arg(request.segmentIndex)
                                 .arg(response.value(
                                          QStringLiteral("state"))
                                          .toString());
        return batch;
    }

    batch.containers = containersFromResponse(response);
    const int responseCount =
        response.value(QStringLiteral("container_count"))
            .toInt(batch.containers.size());
    const int parsedCount = batch.containers.size();
    if (responseCount != request.containerCount
        || parsedCount != request.containerCount)
    {
        qDeleteAll(batch.containers);
        batch.containers.clear();
        QVector<TerminalPickupReservationHandle> handles;
        handles.append(batch.handle);
        releaseReservations(handles);
        batch.blocked = true;
        return batch;
    }

    return batch;
}

bool TerminalPickupCoordinator::commitReservations(
    const QVector<TerminalPickupReservationHandle> &handles,
    double                                          operationTimeSeconds,
    QString                                        *err) const
{
    if (handles.isEmpty())
    {
        if (err)
            err->clear();
        return true;
    }
    if (!m_gateway)
    {
        if (err)
        {
            *err = QStringLiteral(
                "Terminal pickup coordinator requires a terminal inventory gateway");
        }
        return false;
    }

    for (const auto &handle : handles)
    {
        if (!handle.isValid())
        {
            if (err)
                *err = QStringLiteral(
                    "Cannot commit an invalid terminal pickup reservation");
            return false;
        }

        const QJsonObject response =
            m_gateway->commitContainerReservation(
                handle.terminalId,
                handle.reservationId,
                operationTimeSeconds);
        if (response.isEmpty()
            || !responseStateIs(response,
                                QStringLiteral("committed")))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to commit terminal pickup reservation %1 for %2 segment %3")
                           .arg(handle.reservationId,
                                handle.pathIdentity)
                           .arg(handle.segmentIndex);
            }
            return false;
        }
    }

    if (err)
        err->clear();
    return true;
}

bool TerminalPickupCoordinator::releaseReservations(
    const QVector<TerminalPickupReservationHandle> &handles,
    QString                                        *err) const
{
    if (handles.isEmpty())
    {
        if (err)
            err->clear();
        return true;
    }
    if (!m_gateway)
    {
        if (err)
        {
            *err = QStringLiteral(
                "Terminal pickup coordinator requires a terminal inventory gateway");
        }
        return false;
    }

    for (const auto &handle : handles)
    {
        if (!handle.isValid())
            continue;

        const QJsonObject response =
            m_gateway->releaseContainerReservation(
                handle.terminalId,
                handle.reservationId);
        if (response.isEmpty()
            || !responseStateIs(response,
                                QStringLiteral("released")))
        {
            if (err)
            {
                *err = QStringLiteral(
                    "Failed to release terminal pickup reservation %1 for %2 segment %3")
                           .arg(handle.reservationId,
                                handle.pathIdentity)
                           .arg(handle.segmentIndex);
            }
            return false;
        }
    }

    if (err)
        err->clear();
    return true;
}

TerminalPickupReservationHandle TerminalPickupCoordinator::makeHandle(
    const TerminalPickupRequest &request) const
{
    TerminalPickupReservationHandle handle;
    handle.terminalId = request.terminalId;
    handle.reservationId = reservationIdFor(request);
    handle.pathIdentity = request.pathIdentity;
    handle.canonicalPathKey = request.canonicalPathKey;
    handle.segmentIndex = request.segmentIndex;
    handle.containerCount = request.containerCount;
    return handle;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
