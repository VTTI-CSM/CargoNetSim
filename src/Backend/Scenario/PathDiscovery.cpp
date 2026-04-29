#include "PathDiscovery.h"

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/SimulatorCommandAvailability.h"
#include "Backend/Scenario/TerminalGraphBootstrap.h"

#include <QThread>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

namespace {

/// Presence check over `TerminalSimulationClient::getTerminalStatus`.
///
/// The returned pointer is **borrowed from the client's cache**
/// (`m_terminalStatus`) — it is owned by the TerminalSimulationClient,
/// which parents every Terminal to itself in onTerminalAdded /
/// onTerminalsAdded and destroys them on onServerReset /
/// ~TerminalSimulationClient. Do NOT delete it here. An earlier
/// revision of this helper incorrectly `delete`d the pointer on the
/// assumption that `getTerminalStatus` allocated a fresh copy — it
/// does not — and that destroyed the live Terminal while its address
/// was still stored in the cache, leaving every subsequent map reader
/// with a dangling pointer (caught by a SIGSEGV inside
/// `Path::fromJson` during path parsing).
bool isTerminalKnownToServer(TerminalSimulationClient *client,
                             const QString            &terminalId)
{
    return client->getTerminalStatus(terminalId) != nullptr;
}

} // namespace

QList<CargoNetSim::Backend::Path *> PathDiscovery::findTopPaths(
    const ScenarioDocument &doc,
    const ScenarioRegistry &registry,
    int                     n,
    QString                *err)
{
    QList<Path *> result;

    qCInfo(lcScenario) << "PathDiscovery::findTopPaths:"
                       << "topN =" << n;

    // --- Origin/destination pairs from the document ----------------------
    // Short-circuit first: an empty scenario has nothing to discover, so
    // we must not touch the TerminalSim client or RabbitMQ at all — the
    // headless unit contract is "empty document → empty result, no error
    // written". Any later failure IS an error and writes @p err.
    //
    // Task 0 retrofit semantic: an origin is a terminal whose
    // `properties.initial_container_count > 0`. Its destinations come
    // from `destinationsFor()` — scalar `destination_terminal` resolves
    // to a single-entry list, fractioned `destinations: [{t,f}]`
    // resolves to the explicit list. Role-based `findByType("Origin")`
    // is intentionally gone (see Plan 5 top-of-file Option X block).
    const QStringList originIds = doc.originTerminalIds();
    qCInfo(lcScenario) << "PathDiscovery::findTopPaths:"
                       << "origin count =" << originIds.size();
    if (originIds.isEmpty())
    {
        qCWarning(lcScenario) << "PathDiscovery::findTopPaths:"
                              << "no origin terminals — returning early";
        return result;
    }

    // --- Controller + client presence ------------------------------------
    auto &controller     = CargoNetSim::CargoNetSimController::getInstance();
    auto *terminalClient = controller.getTerminalClient();
    if (!terminalClient)
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "TerminalSim client unavailable";
        if (err) *err = QStringLiteral("TerminalSim client unavailable");
        return result;
    }

    if (!isCommandAvailable(terminalClient))
    {
        auto *handler = terminalClient->getRabbitMQHandler();
        auto *clientThread =
            qobject_cast<QThread *>(terminalClient->thread());
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "TerminalSim command queue is unavailable"
                               << "clientThread=" << terminalClient->thread()
                               << "clientThreadRunning="
                               << (clientThread ? clientThread->isRunning()
                                                : false)
                               << "clientConnected="
                               << terminalClient->isConnected()
                               << "handler=" << handler
                               << "handlerConnected="
                               << (handler ? handler->isConnected() : false)
                               << "handlerHasConsumers="
                               << (handler
                                       ? handler->hasCommandQueueConsumers()
                                       : false);
        if (err)
            *err = QStringLiteral(
                "TerminalSim command queue is unavailable");
        return result;
    }

    if (!TerminalGraphBootstrap::resetAndLoad(
            doc, registry, controller, err,
            QStringLiteral("PathDiscovery::findTopPaths")))
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "failed to load TerminalSim baseline"
                               << (err ? *err : QString());
        return result;
    }

    // --- Discover paths per (origin, destination) pair -------------------
    // Fraction values from `destinationsFor()` are intentionally not
    // surfaced here — fractions drive per-pool container distribution
    // at apply time, not path discovery. PathDiscovery only needs the
    // set of distinct endpoints. Option X invariant (2026-04-14): the
    // origin-level DestinationRoute is the single source; per-container
    // destinations do not exist.
    for (const QString &originId : originIds)
    {
        if (!isTerminalKnownToServer(terminalClient, originId))
        {
            qCWarning(lcScenario) << "PathDiscovery::findTopPaths:"
                                  << "origin" << originId
                                  << "not found on graph server";
            if (err) *err = QStringLiteral(
                "Origin terminal '%1' not found in the graph server")
                .arg(originId);
            return result;
        }
        int pathsForOrigin = 0;
        for (const auto &route : doc.destinationsFor(originId))
        {
            const QString &destId = route.terminal;
            if (!isTerminalKnownToServer(terminalClient, destId))
            {
                qCWarning(lcScenario) << "PathDiscovery::findTopPaths:"
                                      << "destination" << destId
                                      << "(from origin" << originId
                                      << ") not found on graph server";
                if (err) *err = QStringLiteral(
                    "Destination terminal '%1' (from origin '%2') "
                    "not found in the graph server")
                    .arg(destId, originId);
                return result;
            }
            qCDebug(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "exploring" << originId << "->" << destId;
            const auto pairPaths = terminalClient->findTopPaths(
                originId, destId, n,
                TransportationTypes::TransportationMode::Any,
                /*skipDelays=*/true);
            pathsForOrigin += pairPaths.size();
            result.append(pairPaths);
        }
        qCDebug(lcScenario) << "PathDiscovery::findTopPaths:"
                            << "origin" << originId
                            << "-> paths found =" << pathsForOrigin;
        if (pathsForOrigin == 0)
            qCWarning(lcScenario) << "PathDiscovery::findTopPaths:"
                                  << "no paths found for origin" << originId;
    }

    qCInfo(lcScenario) << "PathDiscovery::findTopPaths:"
                       << "total paths discovered =" << result.size();

    if (result.isEmpty())
    {
        qCWarning(lcScenario) << "PathDiscovery::findTopPaths:"
                              << "returning with no paths discovered";
        if (err)
            *err = QStringLiteral(
                "No valid paths found between any origin and destination");
    }
    return result;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
