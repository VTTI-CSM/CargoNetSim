#include "TerminalHandoffResolver.h"

#include "ExecutionContainerIdentity.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QString extractScenarioTerminalId(
    const QJsonObject &container,
    QString           *errorMessage)
{
    const QJsonValue directValue =
        container.value(QStringLiteral("scenario_terminal_id"));
    if (directValue.isString()
        && !directValue.toString().trimmed().isEmpty())
    {
        return directValue.toString().trimmed();
    }

    const QJsonObject customVariables =
        container.value(QStringLiteral("customVariables"))
            .toObject();

    QString resolvedId;
    for (auto it = customVariables.constBegin();
         it != customVariables.constEnd(); ++it)
    {
        if (!it.value().isObject())
        {
            continue;
        }

        const QJsonObject haulerVariables =
            it.value().toObject();
        const QJsonValue scenarioTerminalValue =
            haulerVariables.value(
                QStringLiteral("scenario_terminal_id"));
        if (!scenarioTerminalValue.isString())
        {
            continue;
        }

        const QString candidate =
            scenarioTerminalValue.toString().trimmed();
        if (candidate.isEmpty())
        {
            continue;
        }

        if (!resolvedId.isEmpty() && resolvedId != candidate)
        {
            if (errorMessage)
            {
                *errorMessage =
                    QStringLiteral(
                        "Container carries conflicting "
                        "scenario_terminal_id values: '%1' "
                        "and '%2'")
                        .arg(resolvedId, candidate);
            }
            return QString();
        }

        resolvedId = candidate;
    }

    if (resolvedId.isEmpty() && errorMessage)
    {
        *errorMessage = QStringLiteral(
            "Container is missing scenario_terminal_id "
            "metadata");
    }
    return resolvedId;
}

QString extractConsistentStringMetadata(
    const QJsonObject &container,
    const QString     &fieldName,
    QString           *errorMessage)
{
    const QJsonValue directValue = container.value(fieldName);
    if (directValue.isString()
        && !directValue.toString().trimmed().isEmpty())
    {
        return directValue.toString().trimmed();
    }

    const QJsonObject customVariables =
        container.value(QStringLiteral("customVariables"))
            .toObject();

    QString resolvedValue;
    for (auto it = customVariables.constBegin();
         it != customVariables.constEnd(); ++it)
    {
        if (!it.value().isObject())
            continue;

        const QJsonObject haulerVariables = it.value().toObject();
        const QJsonValue fieldValue =
            haulerVariables.value(fieldName);
        if (!fieldValue.isString())
            continue;

        const QString candidate =
            fieldValue.toString().trimmed();
        if (candidate.isEmpty())
            continue;

        if (!resolvedValue.isEmpty()
            && resolvedValue != candidate)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Container carries conflicting %1 values: '%2' and '%3'")
                                    .arg(fieldName, resolvedValue, candidate);
            }
            return QString();
        }

        resolvedValue = candidate;
    }

    if (resolvedValue.isEmpty() && errorMessage)
    {
        *errorMessage = QStringLiteral(
            "Container is missing %1 metadata").arg(fieldName);
    }
    return resolvedValue;
}

int extractConsistentIntMetadata(const QJsonObject &container,
                                 const QString     &fieldName,
                                 QString           *errorMessage)
{
    const QJsonValue directValue = container.value(fieldName);
    if (directValue.isDouble())
        return directValue.toInt();

    const QJsonObject customVariables =
        container.value(QStringLiteral("customVariables"))
            .toObject();

    bool hasValue = false;
    int resolvedValue = -1;
    for (auto it = customVariables.constBegin();
         it != customVariables.constEnd(); ++it)
    {
        if (!it.value().isObject())
            continue;

        const QJsonObject haulerVariables = it.value().toObject();
        const QJsonValue fieldValue =
            haulerVariables.value(fieldName);
        if (!fieldValue.isDouble())
            continue;

        const int candidate = fieldValue.toInt();
        if (hasValue && resolvedValue != candidate)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Container carries conflicting %1 values: '%2' and '%3'")
                                    .arg(fieldName)
                                    .arg(resolvedValue)
                                    .arg(candidate);
            }
            return -1;
        }

        resolvedValue = candidate;
        hasValue = true;
    }

    if (!hasValue && errorMessage)
    {
        *errorMessage = QStringLiteral(
            "Container is missing %1 metadata").arg(fieldName);
    }
    return resolvedValue;
}

QString extractCanonicalPathKeyMetadata(const QJsonObject &container,
                                        QString           *errorMessage)
{
    QString canonicalError;
    const QString canonicalPathKey =
        extractConsistentStringMetadata(
            container,
            ExecutionContainerKeys::canonicalPathKey(),
            &canonicalError);
    if (!canonicalPathKey.isEmpty())
        return canonicalPathKey;

    if (errorMessage)
    {
        *errorMessage = QStringLiteral(
            "Container is missing canonical_path_key metadata");
    }
    return {};
}

} // namespace

TerminalHandoffResolution
resolveTerminalHandoff(const QJsonArray &containers)
{
    TerminalHandoffResolution resolution;
    resolution.containerCount = containers.size();

    if (containers.isEmpty())
    {
        resolution.status = TerminalHandoffResolution::Status::NoOp;
        return resolution;
    }

    QStringList discoveredTerminalIds;
    QString     scenarioTerminalId;
    QString     canonicalPathKey;
    QString     vehicleId;
    int         segmentIndex = -1;

    for (qsizetype index = 0; index < containers.size(); ++index)
    {
        const QJsonValue containerValue = containers.at(index);
        if (!containerValue.isObject())
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral("Unload payload entry %1 is not "
                               "a JSON object")
                    .arg(index);
            return resolution;
        }

        QString containerError;
        const QString candidateId = extractScenarioTerminalId(
            containerValue.toObject(), &containerError);
        if (candidateId.isEmpty())
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral("Unload payload entry %1: %2")
                    .arg(QString::number(index),
                         containerError);
            return resolution;
        }

        if (!scenarioTerminalId.isEmpty()
            && scenarioTerminalId != candidateId)
        {
            discoveredTerminalIds << scenarioTerminalId
                                  << candidateId;
            discoveredTerminalIds.removeDuplicates();

            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral(
                    "Unload payload mixes scenario terminal "
                    "targets: %1")
                    .arg(discoveredTerminalIds.join(", "));
            return resolution;
        }

        scenarioTerminalId = candidateId;

        QString pathKeyError;
        const QString candidatePathKey =
            extractCanonicalPathKeyMetadata(
                containerValue.toObject(), &pathKeyError);
        if (candidatePathKey.isEmpty())
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral("Unload payload entry %1: %2")
                    .arg(QString::number(index), pathKeyError);
            return resolution;
        }
        if (!canonicalPathKey.isEmpty()
            && canonicalPathKey != candidatePathKey)
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral(
                    "Unload payload mixes canonical path keys: %1, %2")
                    .arg(canonicalPathKey, candidatePathKey);
            return resolution;
        }
        canonicalPathKey = candidatePathKey;

        QString vehicleIdError;
        const QString candidateVehicleId =
            extractConsistentStringMetadata(
                containerValue.toObject(),
                ExecutionContainerKeys::vehicleId(),
                &vehicleIdError);
        if (candidateVehicleId.isEmpty())
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral("Unload payload entry %1: %2")
                    .arg(QString::number(index), vehicleIdError);
            return resolution;
        }
        if (!vehicleId.isEmpty() && vehicleId != candidateVehicleId)
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral(
                    "Unload payload mixes vehicle_id values: %1, %2")
                    .arg(vehicleId, candidateVehicleId);
            return resolution;
        }
        vehicleId = candidateVehicleId;

        QString segmentIndexError;
        const int candidateSegmentIndex =
            extractConsistentIntMetadata(
                containerValue.toObject(),
                ExecutionContainerKeys::segmentIndex(),
                &segmentIndexError);
        if (candidateSegmentIndex < 0)
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral("Unload payload entry %1: %2")
                    .arg(QString::number(index),
                         segmentIndexError);
            return resolution;
        }
        if (segmentIndex >= 0
            && segmentIndex != candidateSegmentIndex)
        {
            resolution.status =
                TerminalHandoffResolution::Status::Error;
            resolution.errorMessage =
                QStringLiteral(
                    "Unload payload mixes segment_index values: %1, %2")
                    .arg(segmentIndex)
                    .arg(candidateSegmentIndex);
            return resolution;
        }
        segmentIndex = candidateSegmentIndex;
    }

    QJsonObject containersObject;
    containersObject.insert(QStringLiteral("containers"),
                            containers);

    resolution.status =
        TerminalHandoffResolution::Status::Ready;
    resolution.scenarioTerminalId = scenarioTerminalId;
    resolution.canonicalPathKey = canonicalPathKey;
    resolution.vehicleId = vehicleId;
    resolution.segmentIndex = segmentIndex;
    resolution.containersJson = QString::fromUtf8(
        QJsonDocument(containersObject)
            .toJson(QJsonDocument::Compact));
    return resolution;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
