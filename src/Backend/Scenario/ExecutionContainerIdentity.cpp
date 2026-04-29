#include "ExecutionContainerIdentity.h"

#include <QCryptographicHash>
#include <QJsonArray>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

using Hauler = ContainerCore::Container::HaulerType;

QString key(const char *value)
{
    return QString::fromLatin1(value);
}

void addIfNotEmpty(ContainerCore::Container &container,
                   const QString            &key,
                   const QString            &value)
{
    if (!value.isEmpty())
        ExecutionContainers::addNoHaulerMetadata(container, key, value);
}

void addIfNonNegative(ContainerCore::Container &container,
                      const QString            &key,
                      int                       value)
{
    if (value >= 0)
        ExecutionContainers::addNoHaulerMetadata(container, key, value);
}

} // namespace

namespace ExecutionContainerKeys
{

QString executionId() { return key("execution_id"); }
QString pathIdentity() { return key("path_identity"); }
QString canonicalPathKey() { return key("canonical_path_key"); }
QString sourceContainerId() { return key("source_container_id"); }
QString executionContainerId() { return key("execution_container_id"); }
QString scenarioTerminalId() { return key("scenario_terminal_id"); }
QString runtimeTerminalId() { return key("runtime_terminal_id"); }
QString readySegmentIndex() { return key("ready_segment_index"); }
QString terminalSequenceIndex() { return key("terminal_sequence_index"); }
QString segmentIndex() { return key("segment_index"); }
QString vehicleMode() { return key("vehicle_mode"); }
QString vehicleId() { return key("vehicle_id"); }

} // namespace ExecutionContainerKeys

namespace ExecutionContainers
{

QString makeExecutionContainerId(
    const QString &executionId,
    const QString &pathIdentity,
    const QString &sourceContainerId)
{
    const QByteArray material =
        executionId.toUtf8() + '\x1f' + pathIdentity.toUtf8()
        + '\x1f' + sourceContainerId.toUtf8();
    const QByteArray digest =
        QCryptographicHash::hash(material,
                                 QCryptographicHash::Sha256)
            .toHex();
    return QStringLiteral("ec|v1|sig=%1")
        .arg(QString::fromLatin1(digest));
}

QString customVariableString(
    const ContainerCore::Container &container,
    const QString                  &key)
{
    return container.getCustomVariable(Hauler::noHauler, key)
        .toString();
}

QString sourceContainerIdFor(
    const ContainerCore::Container &container)
{
    const QString sourceId = customVariableString(
        container, ExecutionContainerKeys::sourceContainerId());
    return sourceId.isEmpty() ? container.getContainerID()
                              : sourceId;
}

QString logicalContainerIdFor(
    const ContainerCore::Container &container)
{
    const QString executionId = customVariableString(
        container, ExecutionContainerKeys::executionContainerId());
    return executionId.isEmpty() ? container.getContainerID()
                                 : executionId;
}

ExecutionContainerMetadata makeIdentityMetadata(
    const QString                  &executionId,
    const QString                  &pathIdentity,
    const QString                  &canonicalPathKey,
    const ContainerCore::Container &sourceContainer,
    int                             readySegmentIndex,
    int                             terminalSequenceIndex)
{
    ExecutionContainerMetadata metadata;
    metadata.executionId = executionId;
    metadata.pathIdentity = pathIdentity;
    metadata.canonicalPathKey = canonicalPathKey;
    metadata.sourceContainerId =
        sourceContainerIdFor(sourceContainer);
    metadata.executionContainerId = makeExecutionContainerId(
        executionId, pathIdentity, metadata.sourceContainerId);
    metadata.readySegmentIndex = readySegmentIndex;
    metadata.terminalSequenceIndex = terminalSequenceIndex;
    return metadata;
}

ContainerCore::Container *makeExecutionContainerCopy(
    const ContainerCore::Container  &sourceContainer,
    const ExecutionContainerMetadata &metadata,
    const QString                   &currentTerminalId)
{
    auto *copy = sourceContainer.copy();
    if (!metadata.executionContainerId.isEmpty())
        copy->setContainerID(metadata.executionContainerId);
    if (!currentTerminalId.isEmpty())
        copy->setContainerCurrentLocation(currentTerminalId);
    applyIdentityMetadata(*copy, metadata);
    return copy;
}

void addNoHaulerMetadata(
    ContainerCore::Container &container,
    const QString            &key,
    const QVariant           &value)
{
    container.addCustomVariable(Hauler::noHauler, key, value);
}

void applyIdentityMetadata(
    ContainerCore::Container        &container,
    const ExecutionContainerMetadata &metadata)
{
    addIfNotEmpty(container, ExecutionContainerKeys::executionId(),
                  metadata.executionId);
    addIfNotEmpty(container, ExecutionContainerKeys::pathIdentity(),
                  metadata.pathIdentity);
    addIfNotEmpty(container, ExecutionContainerKeys::canonicalPathKey(),
                  metadata.canonicalPathKey);
    addIfNotEmpty(container, ExecutionContainerKeys::sourceContainerId(),
                  metadata.sourceContainerId);
    addIfNotEmpty(container, ExecutionContainerKeys::executionContainerId(),
                  metadata.executionContainerId);
    addIfNonNegative(container, ExecutionContainerKeys::readySegmentIndex(),
                     metadata.readySegmentIndex);
    addIfNonNegative(container,
                     ExecutionContainerKeys::terminalSequenceIndex(),
                     metadata.terminalSequenceIndex);
}

void applyDispatchMetadata(
    ContainerCore::Container        &container,
    const ExecutionContainerMetadata &metadata)
{
    applyIdentityMetadata(container, metadata);
    addIfNotEmpty(container, ExecutionContainerKeys::scenarioTerminalId(),
                  metadata.scenarioTerminalId);
    addIfNotEmpty(container, ExecutionContainerKeys::runtimeTerminalId(),
                  metadata.runtimeTerminalId);
    addIfNonNegative(container, ExecutionContainerKeys::segmentIndex(),
                     metadata.segmentIndex);
    addIfNotEmpty(container, ExecutionContainerKeys::vehicleMode(),
                  metadata.vehicleMode);
    addIfNotEmpty(container, ExecutionContainerKeys::vehicleId(),
                  metadata.vehicleId);
}

void replaceNextDestination(
    ContainerCore::Container &container,
    const QString            &destination)
{
    QVector<QString> destinations;
    if (!destination.isEmpty())
        destinations.append(destination);
    container.setContainerNextDestinations(destinations);
}

QJsonObject customVariableFilter(
    const QString  &key,
    const QVariant &value)
{
    QJsonObject filter;
    filter.insert(QStringLiteral("hauler"),
                  static_cast<int>(Hauler::noHauler));
    filter.insert(QStringLiteral("key"), key);
    filter.insert(QStringLiteral("value"),
                  QJsonValue::fromVariant(value));
    return filter;
}

QJsonObject terminalPickupCriteria(
    const QString &executionId,
    const QString &pathIdentity,
    int            readySegmentIndex,
    const QString &canonicalPathKey,
    int            limit)
{
    QJsonArray filters;
    filters.append(customVariableFilter(
        ExecutionContainerKeys::executionId(), executionId));
    filters.append(customVariableFilter(
        ExecutionContainerKeys::pathIdentity(), pathIdentity));
    filters.append(customVariableFilter(
        ExecutionContainerKeys::readySegmentIndex(),
        readySegmentIndex));
    if (!canonicalPathKey.isEmpty())
    {
        filters.append(customVariableFilter(
            ExecutionContainerKeys::canonicalPathKey(),
            canonicalPathKey));
    }

    QJsonObject criteria;
    criteria.insert(QStringLiteral("custom_variables"), filters);
    criteria.insert(QStringLiteral("sort_by"),
                    QStringLiteral("container_id"));
    criteria.insert(QStringLiteral("sort_ascending"), true);
    if (limit >= 0)
        criteria.insert(QStringLiteral("limit"), limit);
    return criteria;
}

} // namespace ExecutionContainers

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
