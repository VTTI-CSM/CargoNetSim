#include "TerminalHandoffResolver.h"

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
    }

    QJsonObject containersObject;
    containersObject.insert(QStringLiteral("containers"),
                            containers);

    resolution.status =
        TerminalHandoffResolution::Status::Ready;
    resolution.scenarioTerminalId = scenarioTerminalId;
    resolution.containersJson = QString::fromUtf8(
        QJsonDocument(containersObject)
            .toJson(QJsonDocument::Compact));
    return resolution;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
