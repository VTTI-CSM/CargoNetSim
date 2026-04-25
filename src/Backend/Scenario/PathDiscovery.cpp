#include "PathDiscovery.h"

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/SimulatorCommandAvailability.h"

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

namespace {

// Type alias for the nested enum class — `TransportationTypes` is a
// class (with a Q_ENUM-registered `TransportationMode` inside) rather
// than a namespace, so the using-declaration form is awkward. Matches
// the pattern in `ScenarioApplier.cpp:197`.
using Mode = TransportationTypes::TransportationMode;

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

/// Human-readable stable id for a `PathSegment`. The segment id is
/// required to be non-empty (`PathSegment.h:48-49` throws on empty)
/// but has no uniqueness contract outside of this list — synthesising
/// `"<from>-><to>:<mode>"` is enough because every `(from, to, mode)`
/// tuple appears at most once across `doc.connections + doc.globalLinks`
/// (ScenarioValidator enforces this at schema-load time).
///
/// Uses `transportationModeToString` (lowercase internal vocabulary —
/// `"truck"` / `"rail"` / `"ship"`), NOT `interfaceModeCanonicalString`
/// (`"Truck"` / `"Rail"` / `"Ship"` — reserved for YAML schema
/// boundaries). The segment id is an internal identifier, not a
/// schema value, so the lowercase form is appropriate.
QString makeSegmentId(const QString &from, const QString &to,
                      Mode           mode)
{
    return QStringLiteral("%1->%2:%3")
        .arg(from, to, transportationModeToString(mode));
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
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "TerminalSim command queue is unavailable";
        if (err)
            *err = QStringLiteral(
                "TerminalSim command queue is unavailable");
        return result;
    }

    // --- Reset server + configure cost weights ---------------------------
    // Equivalent to PathFindingWorker.cpp:52-67. Reads the *authoritative*
    // weights from ConfigController (populated by ScenarioApplier's
    // applySettings step — same weights the GUI sees).
    if (!terminalClient->resetServer())
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "failed to reset terminal server";
        if (err) *err = QStringLiteral("Failed to reset terminal server");
        return result;
    }
    auto *config = controller.getConfigController();
    if (!config)
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "ConfigController unavailable";
        if (err) *err = QStringLiteral("ConfigController unavailable");
        return result;
    }
    terminalClient->setCostFunctionParameters(
        config->getCostFunctionWeights());

    // --- Push terminals to the server ------------------------------------
    // Reuse Plan 2's registry-populated `Backend::Terminal*`s — no local
    // reconstruction (the 136-line createTerminalObject that lived in
    // the pre-Plan-5 PathFindingWorker is gone; registry is the single
    // source of truth).
    QList<Terminal *> terminals;
    terminals.reserve(doc.terminals.size());
    for (auto it = doc.terminals.constBegin();
         it != doc.terminals.constEnd(); ++it)
    {
        if (Terminal *t = registry.terminal(it.key()))
            terminals.append(t);
    }
    qCDebug(lcScenario) << "PathDiscovery::findTopPaths:"
                        << "pushing" << terminals.size()
                        << "terminals to server";
    if (!terminalClient->addTerminals(terminals))
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "failed to add terminals to server";
        if (err) *err = QStringLiteral("Failed to add terminals to server");
        return result;
    }

    // --- Push routes (regional connections + cross-region links) ---------
    // Connection::mode and GlobalLink::mode are typed TransportationMode
    // enums (Plan 2 / Plan 7) — pass them through directly. PathSegment
    // takes ownership-agnostic pointers; we delete them after the bulk
    // add call.
    QList<PathSegment *> routes;
    routes.reserve(doc.connections.size() + doc.globalLinks.size());
    for (const auto &c : doc.connections)
    {
        const QStringList missingKeys =
            RouteMetricUnits::missingCanonicalRouteMetricKeys(
                c.properties);
        if (!missingKeys.isEmpty())
        {
            qCWarning(lcScenario)
                << "PathDiscovery::findTopPaths:"
                << "regional connection" << c.fromTerminalId
                << "->" << c.toTerminalId
                << "is missing canonical route metrics"
                << missingKeys;
        }
        routes.append(new PathSegment(
            makeSegmentId(c.fromTerminalId, c.toTerminalId, c.mode),
            c.fromTerminalId, c.toTerminalId, c.mode,
            RouteMetricUnits::routeAttributesFromCanonical(
                c.properties)));
    }
    for (const auto &gl : doc.globalLinks)
    {
        const QStringList missingKeys =
            RouteMetricUnits::missingCanonicalRouteMetricKeys(
                gl.properties);
        if (!missingKeys.isEmpty())
        {
            qCWarning(lcScenario)
                << "PathDiscovery::findTopPaths:"
                << "global link" << gl.fromTerminalId
                << "->" << gl.toTerminalId
                << "is missing canonical route metrics"
                << missingKeys;
        }
        routes.append(new PathSegment(
            makeSegmentId(gl.fromTerminalId, gl.toTerminalId, gl.mode),
            gl.fromTerminalId, gl.toTerminalId, gl.mode,
            RouteMetricUnits::routeAttributesFromCanonical(
                gl.properties)));
    }
    qCDebug(lcScenario) << "PathDiscovery::findTopPaths:"
                        << "pushing" << routes.size() << "routes to server"
                        << "(" << doc.connections.size() << "connections +"
                        << doc.globalLinks.size() << "global links)";
    const bool routesOk = terminalClient->addRoutes(routes);
    qDeleteAll(routes);
    if (!routesOk)
    {
        qCCritical(lcScenario) << "PathDiscovery::findTopPaths:"
                               << "failed to add routes to server";
        if (err) *err = QStringLiteral("Failed to add routes to server");
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
