#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

struct ExecutionContainerMetadata
{
    QString executionId;
    QString executionPathKey;
    QString canonicalPathKey;
    QString sourceContainerId;
    QString executionContainerId;
    QString scenarioTerminalId;
    QString runtimeTerminalId;
    int     readySegmentIndex = -1;
    int     terminalSequenceIndex = -1;
    int     segmentIndex = -1;
    QString vehicleMode;
    QString vehicleId;
};

namespace ExecutionContainerKeys
{

QString executionId();
QString canonicalPathKey();
QString sourceContainerId();
QString executionContainerId();
QString scenarioTerminalId();
QString runtimeTerminalId();
QString readySegmentIndex();
QString terminalSequenceIndex();
QString segmentIndex();
QString vehicleMode();
QString vehicleId();

} // namespace ExecutionContainerKeys

namespace ExecutionContainers
{

QString makeExecutionContainerId(
    const QString &executionId,
    const QString &executionPathKey,
    const QString &sourceContainerId);

QString customVariableString(
    const ContainerCore::Container &container,
    const QString                  &key);

QString sourceContainerIdFor(
    const ContainerCore::Container &container);

QString logicalContainerIdFor(
    const ContainerCore::Container &container);

ExecutionContainerMetadata makeIdentityMetadata(
    const QString                  &executionId,
    const QString                  &executionPathKey,
    const QString                  &canonicalPathKey,
    const ContainerCore::Container &sourceContainer,
    int                             readySegmentIndex,
    int                             terminalSequenceIndex);

ContainerCore::Container *makeExecutionContainerCopy(
    const ContainerCore::Container &sourceContainer,
    const ExecutionContainerMetadata &metadata,
    const QString                  &currentTerminalId = QString());

void addNoHaulerMetadata(
    ContainerCore::Container &container,
    const QString            &key,
    const QVariant           &value);

void applyIdentityMetadata(
    ContainerCore::Container        &container,
    const ExecutionContainerMetadata &metadata);

void applyDispatchMetadata(
    ContainerCore::Container        &container,
    const ExecutionContainerMetadata &metadata);

void replaceNextDestination(
    ContainerCore::Container &container,
    const QString            &destination);

QJsonObject customVariableFilter(
    const QString  &key,
    const QVariant &value);

QJsonObject terminalPickupCriteria(
    const QString &executionId,
    const QString &canonicalPathKey,
    int            readySegmentIndex,
    int            limit = -1);

} // namespace ExecutionContainers

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
