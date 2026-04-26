#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
}

namespace Application
{

struct BackendAvailabilityStatus
{
    QString serverKey;
    QString server;
    bool    clientExists = false;
    bool    connected = false;
    bool    hasConsumers = false;
    bool    commandAvailable = false;
};

struct AvailabilityWaitResult
{
    QList<BackendAvailabilityStatus> statuses;
    QStringList                      missingServerKeys;
    bool                             satisfied = false;
};

class AvailabilityService
{
public:
    AvailabilityService() = default;

    QList<BackendAvailabilityStatus> pollAll() const;

    AvailabilityWaitResult waitForCommandAvailability(
        const QStringList &requiredServerKeys,
        int                timeoutMs      = 5000,
        int                pollIntervalMs = 50) const;

    static QString canonicalServerKey(const QString &serverName);

    static QStringList requiredServerKeysForScenario(
        const Scenario::ScenarioDocument &document);

    static QStringList missingCommandServers(
        const QStringList                    &requiredServerKeys,
        const QList<BackendAvailabilityStatus> &statuses);
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
