#include "AvailabilityService.h"

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ServerStatusProbe.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSet>
#include <QThread>

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

QStringList uniqueRequiredKeys(const QStringList &requiredServerKeys)
{
    QStringList uniqueKeys;
    uniqueKeys.reserve(requiredServerKeys.size());
    for (const auto &key : requiredServerKeys)
    {
        if (!uniqueKeys.contains(key))
            uniqueKeys.append(key);
    }
    return uniqueKeys;
}

} // namespace

QList<BackendAvailabilityStatus>
AvailabilityService::pollAll() const
{
    Scenario::ServerStatusProbe probe;
    const auto                  statuses = probe.pollAll();

    QList<BackendAvailabilityStatus> result;
    result.reserve(statuses.size());
    for (const auto &status : statuses)
    {
        BackendAvailabilityStatus mapped;
        mapped.serverKey = canonicalServerKey(status.server);
        mapped.server = status.server;
        mapped.clientExists = status.clientExists;
        mapped.connected = status.connected;
        mapped.hasConsumers = status.hasConsumers;
        mapped.commandAvailable = status.commandAvailable;
        result.append(mapped);
    }

    return result;
}

AvailabilityWaitResult AvailabilityService::waitForCommandAvailability(
    const QStringList &requiredServerKeys, int timeoutMs,
    int pollIntervalMs) const
{
    AvailabilityWaitResult result;
    const QStringList      requiredKeys =
        uniqueRequiredKeys(requiredServerKeys);

    QElapsedTimer timer;
    timer.start();

    while (true)
    {
        result.statuses = pollAll();
        result.missingServerKeys =
            missingCommandServers(requiredKeys, result.statuses);
        result.satisfied = result.missingServerKeys.isEmpty();
        if (result.satisfied)
            return result;

        if (timer.elapsed() >= timeoutMs)
            return result;

        QCoreApplication::processEvents(QEventLoop::AllEvents,
                                        pollIntervalMs);
        QThread::msleep(static_cast<unsigned long>(
            pollIntervalMs > 0 ? pollIntervalMs : 1));
    }
}

QString AvailabilityService::canonicalServerKey(
    const QString &serverName)
{
    if (serverName == QStringLiteral("TerminalSim"))
        return QStringLiteral("terminal");
    if (serverName == QStringLiteral("NeTrainSim"))
        return QStringLiteral("train");
    if (serverName == QStringLiteral("ShipNetSim"))
        return QStringLiteral("ship");
    if (serverName == QStringLiteral("INTEGRATION"))
        return QStringLiteral("truck");
    return serverName.trimmed().toLower();
}

QStringList AvailabilityService::requiredServerKeysForScenario(
    const Scenario::ScenarioDocument &document)
{
    using Mode = Backend::TransportationTypes::TransportationMode;

    QSet<QString> requiredKeys = {QStringLiteral("terminal")};
    const auto addModeKey = [&requiredKeys](Mode mode) {
        switch (mode)
        {
        case Mode::Train:
            requiredKeys.insert(QStringLiteral("train"));
            break;
        case Mode::Ship:
            requiredKeys.insert(QStringLiteral("ship"));
            break;
        case Mode::Truck:
            requiredKeys.insert(QStringLiteral("truck"));
            break;
        default:
            break;
        }
    };

    for (const auto &connection : document.connections)
        addModeKey(connection.mode);
    for (const auto &globalLink : document.globalLinks)
        addModeKey(globalLink.mode);

    return QStringList(requiredKeys.begin(), requiredKeys.end());
}

QStringList AvailabilityService::missingCommandServers(
    const QStringList &requiredServerKeys,
    const QList<BackendAvailabilityStatus> &statuses)
{
    QHash<QString, bool> commandAvailabilityByKey;
    for (const auto &status : statuses)
        commandAvailabilityByKey.insert(status.serverKey,
                                        status.commandAvailable);

    QStringList missingServerKeys;
    missingServerKeys.reserve(requiredServerKeys.size());
    for (const auto &requiredKey : requiredServerKeys)
    {
        if (!commandAvailabilityByKey.value(requiredKey, false))
            missingServerKeys.append(requiredKey);
    }

    return missingServerKeys;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
