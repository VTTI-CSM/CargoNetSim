#include "ScenarioLinker.h"

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/GeoDistance.h"
#include "Backend/Commons/LogCategories.h"
#include "InterfaceConversion.h"
#include "NetworkLookup.h"
#include "TerminalTypeDefaults.h"

#include <QMap>
#include <QPair>
#include <QSet>
#include <QtMath>
#include <cmath>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

// Three file-local registries. Populated by TU-static initializers in the
// per-rule tasks below.
QMap<QString, LinkageRuleFn>    &linkageRegistry()
{
    static QMap<QString, LinkageRuleFn> r;
    return r;
}
QMap<QString, ConnectionRuleFn> &connectionRegistry()
{
    static QMap<QString, ConnectionRuleFn> r;
    return r;
}
QMap<QString, GlobalRuleFn>     &globalRegistry()
{
    static QMap<QString, GlobalRuleFn> r;
    return r;
}

// Helpers for rule registration (used by static initializers added in later tasks).
struct LinkageRuleRegistrar
{
    LinkageRuleRegistrar(const QString &name, LinkageRuleFn fn)
    { linkageRegistry().insert(name, std::move(fn)); }
};
struct ConnectionRuleRegistrar
{
    ConnectionRuleRegistrar(const QString &name, ConnectionRuleFn fn)
    { connectionRegistry().insert(name, std::move(fn)); }
};
struct GlobalRuleRegistrar
{
    GlobalRuleRegistrar(const QString &name, GlobalRuleFn fn)
    { globalRegistry().insert(name, std::move(fn)); }
};

// Default maximum search distance in scene units for nearest-node lookup.
// Matches the GUI's implicit "reasonable proximity" heuristic without
// pulling in QApplication/MapPoint. Tune if fixtures require it.
constexpr double kMaxNearestDistance = 1.0e7;

double sceneDistance(double ax, double ay, double bx, double by)
{
    const double dx = ax - bx;
    const double dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// Helper: given a terminal placement, return its (sceneX, sceneY). For
// LatLon / NetworkNode placements, a best-effort conversion is applied.
QPair<double, double> terminalSceneCoords(const TerminalPlacement &t,
                                          const TrainClient::NeTrainSimNetwork *railForResolve)
{
    switch (t.mode)
    {
    case TerminalPlacement::PositionMode::Scene:
        return { t.scenePos.x, t.scenePos.y };
    case TerminalPlacement::PositionMode::NetworkNode:
        if (railForResolve)
        {
            // Best-effort: look up the referenced node in the rail network
            // when compatible. NetworkNode-mode terminals anchored to a
            // non-rail network fall through to (0, 0) for rule purposes.
            const auto nodes = railForResolve->getNodes();
            for (auto *n : nodes)
                if (n && n->getUserId() == t.nodeId)
                    return { n->getX(), n->getY() };
        }
        return { 0.0, 0.0 };
    case TerminalPlacement::PositionMode::LatLon:
        // Fallback: use lat/lon as raw XY when no projection resolver is
        // available.
        return { t.latLon.longitude, t.latLon.latitude };
    }
    return { 0.0, 0.0 };
}

// Network lookup moved to NetworkLookup.{h,cpp} as the single source of
// truth shared by linking, execution planning, and dispatch construction.
// See NetworkLookup::collectRail / collectTruck for the preview-vs-live
// resolution policy.

LinkageRuleFn landTerminalToRailNodeRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &registry,
              const QString &region) -> QList<NodeLinkage>
    {
        QList<NodeLinkage> out;
        if (!doc.regions.contains(region)) return out;

        const auto railByName = NetworkLookup::collectRail(registry, region);

        // For each rail network in the region…
        for (auto it = railByName.constBegin(); it != railByName.constEnd(); ++it)
        {
            const QString &railName = it.key();
            auto *rail = it.value();
            if (!rail) continue;
            const auto nodes = rail->getNodes();
            if (nodes.isEmpty()) continue;

            // For each Intermodal Land Terminal in the region…
            for (const TerminalPlacement &t : doc.terminals.values())
            {
                if (t.region != region) continue;
                if (t.type != QLatin1String("Intermodal Land Terminal")) continue;

                qCDebug(lcScenario) << "landTerminalToRailNodeRule:"
                                    << "evaluating terminal" << t.id
                                    << "against rail" << railName;
                const auto coords = terminalSceneCoords(t, rail);
                double bestDist = kMaxNearestDistance;
                int    bestId   = -1;
                for (auto *n : nodes)
                {
                    if (!n) continue;
                    const double d = sceneDistance(coords.first, coords.second,
                                                   n->getX(),     n->getY());
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestId   = n->getUserId();
                    }
                }
                if (bestId >= 0)
                {
                    NodeLinkage l;
                    l.terminalId  = t.id;
                    l.networkName = railName;
                    l.nodeId      = bestId;
                    l.source      = LinkageSource::Auto;
                    out.append(l);
                    qCDebug(lcScenario) << "landTerminalToRailNodeRule:"
                                        << "linkage created:" << t.id
                                        << "->" << railName << "node" << bestId;
                }
                else
                {
                    qCWarning(lcScenario) << "landTerminalToRailNodeRule:"
                                          << "no matching rail node for terminal" << t.id;
                }
            }
        }
        return out;
    };
}

// TU-static registration (runs before main).
static LinkageRuleRegistrar s_landTerminalToRailNode(
    "land_terminal_to_rail_node", landTerminalToRailNodeRule());

// Sibling of terminalSceneCoords for truck-side networks. The accessor
// names differ on IntegrationNode (getXCoordinate / getYCoordinate /
// getNodeId) vs. NeTrainSimNode (getX / getY / getUserId), so a templated
// helper would add cast noise. Two parallel ~20-line helpers are easier
// to audit.
QPair<double, double> terminalSceneCoordsForTruck(
    const TerminalPlacement &t,
    const TruckClient::IntegrationNetwork *truckForResolve)
{
    switch (t.mode)
    {
    case TerminalPlacement::PositionMode::Scene:
        return { t.scenePos.x, t.scenePos.y };
    case TerminalPlacement::PositionMode::NetworkNode:
        if (truckForResolve)
        {
            const auto nodes = truckForResolve->getNodes();
            for (auto *n : nodes)
                if (n && n->getNodeId() == t.nodeId)
                    return { static_cast<double>(n->getXCoordinate()),
                             static_cast<double>(n->getYCoordinate()) };
        }
        return { 0.0, 0.0 };
    case TerminalPlacement::PositionMode::LatLon:
        return { t.latLon.longitude, t.latLon.latitude };
    }
    return { 0.0, 0.0 };
}

LinkageRuleFn truckParkingToTruckNodeRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &registry,
              const QString &region) -> QList<NodeLinkage>
    {
        QList<NodeLinkage> out;
        if (!doc.regions.contains(region)) return out;

        const auto truckByName = NetworkLookup::collectTruck(registry, region);

        for (auto it = truckByName.constBegin(); it != truckByName.constEnd(); ++it)
        {
            const QString &truckName = it.key();
            auto *truck = it.value();
            if (!truck) continue;
            const auto nodes = truck->getNodes();
            if (nodes.isEmpty()) continue;

            for (const TerminalPlacement &t : doc.terminals.values())
            {
                if (t.region != region) continue;
                if (t.type != QLatin1String("Truck Parking")
                    && t.type != QLatin1String("Facility"))
                    continue;

                qCDebug(lcScenario) << "truckParkingToTruckNodeRule:"
                                    << "evaluating terminal" << t.id
                                    << "against truck" << truckName;
                const auto coords = terminalSceneCoordsForTruck(t, truck);
                double bestDist = kMaxNearestDistance;
                int    bestId   = -1;
                for (auto *n : nodes)
                {
                    if (!n) continue;
                    const double d = sceneDistance(coords.first, coords.second,
                                                   static_cast<double>(n->getXCoordinate()),
                                                   static_cast<double>(n->getYCoordinate()));
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestId   = n->getNodeId();
                    }
                }
                if (bestId >= 0)
                {
                    NodeLinkage l;
                    l.terminalId  = t.id;
                    l.networkName = truckName;
                    l.nodeId      = bestId;
                    l.source      = LinkageSource::Auto;
                    out.append(l);
                    qCDebug(lcScenario) << "truckParkingToTruckNodeRule:"
                                        << "linkage created:" << t.id
                                        << "->" << truckName << "node" << bestId;
                }
                else
                {
                    qCWarning(lcScenario) << "truckParkingToTruckNodeRule:"
                                          << "no matching truck node for terminal" << t.id;
                }
            }
        }
        return out;
    };
}

static LinkageRuleRegistrar s_truckParkingToTruckNode(
    "truck_parking_to_truck_node", truckParkingToTruckNodeRule());

// Sea Port Terminal → nearest rail node. Same pattern as
// land_terminal_to_rail_node with only the terminal-type filter changed;
// kept parallel per the spec's rule catalog (§7.2). A future refactor may
// collapse them into a single parameterized rule.
LinkageRuleFn seaPortToNearestRailRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &registry,
              const QString &region) -> QList<NodeLinkage>
    {
        QList<NodeLinkage> out;
        if (!doc.regions.contains(region)) return out;

        const auto railByName = NetworkLookup::collectRail(registry, region);

        for (auto it = railByName.constBegin(); it != railByName.constEnd(); ++it)
        {
            const QString &railName = it.key();
            auto *rail = it.value();
            if (!rail) continue;
            const auto nodes = rail->getNodes();
            if (nodes.isEmpty()) continue;

            for (const TerminalPlacement &t : doc.terminals.values())
            {
                if (t.region != region) continue;
                if (t.type != QLatin1String("Sea Port Terminal")) continue;

                qCDebug(lcScenario) << "seaPortToNearestRailRule:"
                                    << "evaluating terminal" << t.id
                                    << "against rail" << railName;
                const auto coords = terminalSceneCoords(t, rail);
                double bestDist = kMaxNearestDistance;
                int    bestId   = -1;
                for (auto *n : nodes)
                {
                    if (!n) continue;
                    const double d = sceneDistance(coords.first, coords.second,
                                                   n->getX(),     n->getY());
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestId   = n->getUserId();
                    }
                }
                if (bestId >= 0)
                {
                    NodeLinkage l;
                    l.terminalId  = t.id;
                    l.networkName = railName;
                    l.nodeId      = bestId;
                    l.source      = LinkageSource::Auto;
                    out.append(l);
                    qCDebug(lcScenario) << "seaPortToNearestRailRule:"
                                        << "linkage created:" << t.id
                                        << "->" << railName << "node" << bestId;
                }
                else
                {
                    qCWarning(lcScenario) << "seaPortToNearestRailRule:"
                                          << "no matching rail node for terminal" << t.id;
                }
            }
        }
        return out;
    };
}

static LinkageRuleRegistrar s_seaPortToNearestRail(
    "sea_port_to_nearest_rail", seaPortToNearestRailRule());

ConnectionRuleFn byNetworksRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &,
              const QString &region) -> QList<Connection>
    {
        QList<Connection> out;

        // Group linkages by (terminal, networkName).
        QMap<QString, QSet<QString>> networksPerTerminal;
        for (const NodeLinkage &l : doc.linkages)
        {
            if (!doc.terminals.contains(l.terminalId)) continue;
            if (doc.terminals[l.terminalId].region != region) continue;
            networksPerTerminal[l.terminalId].insert(l.networkName);
        }

        const QStringList terminalIds = networksPerTerminal.keys();
        for (int i = 0; i < terminalIds.size(); ++i)
        {
            for (int j = i + 1; j < terminalIds.size(); ++j)
            {
                const auto shared = networksPerTerminal[terminalIds[i]]
                                        & networksPerTerminal[terminalIds[j]];
                if (shared.isEmpty()) continue;

                Connection c;
                c.fromTerminalId = terminalIds[i];
                c.toTerminalId   = terminalIds[j];
                c.mode = TransportationTypes::TransportationMode::Train;  // by_networks default; refined by by_interfaces if both rules present
                c.region         = region;
                c.source         = LinkageSource::Auto;
                out.append(c);
                qCDebug(lcScenario) << "byNetworksRule:"
                                    << "connection created:" << terminalIds[i]
                                    << "->" << terminalIds[j] << "in" << region;
            }
        }
        return out;
    };
}

static ConnectionRuleRegistrar s_byNetworks("by_networks", byNetworksRule());

ConnectionRuleFn byInterfacesRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &registry,
              const QString &region) -> QList<Connection>
    {
        QList<Connection> out;

        // For each pair of terminals in this region, intersect backend-form
        // interface sets. Mode preference order: rail > truck > ship.
        QList<const TerminalPlacement *> ts;
        for (auto it = doc.terminals.constBegin(); it != doc.terminals.constEnd(); ++it)
            if (it.value().region == region) ts.append(&it.value());

        auto backendForTerminal = [](const TerminalPlacement &t)
        {
            // InterfaceSet is typed, so build the backend map directly
            // without a string intermediate.
            using Mode = TransportationTypes::TransportationMode;
            QSet<Mode> land, sea;
            if (t.interfaces.isSet) { land = t.interfaces.landSide; sea = t.interfaces.seaSide; }
            else
            {
                const auto defaults = TerminalTypeDefaults::interfacesFor(t.type);
                land = defaults.first; sea = defaults.second;
            }
            InterfaceConversion::InterfaceMap map;
            if (!land.isEmpty())
                map.insert(TerminalTypes::TerminalInterface::LAND_SIDE, land);
            if (!sea.isEmpty())
                map.insert(TerminalTypes::TerminalInterface::SEA_SIDE, sea);
            return map;
        };

        for (int i = 0; i < ts.size(); ++i)
        {
            for (int j = i + 1; j < ts.size(); ++j)
            {
                auto mi = backendForTerminal(*ts[i]);
                auto mj = backendForTerminal(*ts[j]);

                // Find the first shared mode in preference order.
                using Mode = TransportationTypes::TransportationMode;
                auto has = [&](Mode m) -> bool
                {
                    auto all = mi[TerminalTypes::TerminalInterface::LAND_SIDE] +
                               mi[TerminalTypes::TerminalInterface::SEA_SIDE];
                    auto all2 = mj[TerminalTypes::TerminalInterface::LAND_SIDE] +
                                mj[TerminalTypes::TerminalInterface::SEA_SIDE];
                    return all.contains(m) && all2.contains(m);
                };

                Mode mode;
                if      (has(Mode::Train)) mode = Mode::Train;
                else if (has(Mode::Truck)) mode = Mode::Truck;
                else if (has(Mode::Ship))  mode = Mode::Ship;
                else continue;

                Connection c;
                c.fromTerminalId = ts[i]->id;
                c.toTerminalId   = ts[j]->id;
                c.mode           = mode;
                c.region         = region;
                c.source         = LinkageSource::Auto;
                out.append(c);
                qCDebug(lcScenario) << "byInterfacesRule:"
                                    << "connection created:" << ts[i]->id
                                    << "->" << ts[j]->id
                                    << "mode=" << static_cast<int>(mode)
                                    << "in" << region;
            }
        }
        (void)registry;
        return out;
    };
}

static ConnectionRuleRegistrar s_byInterfaces("by_interfaces", byInterfacesRule());

GlobalRuleFn seaPortPairsWithinKmRule()
{
    return [](const ScenarioDocument &doc,
              const ScenarioRegistry &,
              const QVariant &param) -> QList<GlobalLink>
    {
        QList<GlobalLink> out;
        bool ok = false;
        const double kmThreshold = param.toDouble(&ok);
        if (!ok || kmThreshold <= 0.0) return out;

        auto latLonOf = [&](const TerminalPlacement &t) -> QPair<double, double>
        {
            if (t.mode == TerminalPlacement::PositionMode::LatLon)
                return { t.latLon.latitude, t.latLon.longitude };
            if (doc.regions.contains(t.region))
            {
                const auto &r = doc.regions[t.region];
                return { r.globalPosition.latitude, r.globalPosition.longitude };
            }
            return { 0, 0 };
        };

        QList<const TerminalPlacement *> seaPorts;
        for (auto it = doc.terminals.constBegin(); it != doc.terminals.constEnd(); ++it)
            if (it.value().type == QLatin1String("Sea Port Terminal"))
                seaPorts.append(&it.value());

        for (int i = 0; i < seaPorts.size(); ++i)
        {
            for (int j = i + 1; j < seaPorts.size(); ++j)
            {
                if (seaPorts[i]->region == seaPorts[j]->region) continue;

                const auto p = latLonOf(*seaPorts[i]);
                const auto q = latLonOf(*seaPorts[j]);
                const double d = CargoNetSim::Backend::Commons::GeoDistance::haversineKm(
                    p.first, p.second, q.first, q.second);
                if (d > kmThreshold) continue;

                GlobalLink g;
                g.fromTerminalId = seaPorts[i]->region + "/" + seaPorts[i]->id;
                g.toTerminalId   = seaPorts[j]->region + "/" + seaPorts[j]->id;
                g.mode           = TransportationTypes::TransportationMode::Ship;
                g.source         = LinkageSource::Auto;
                out.append(g);
                qCDebug(lcScenario) << "seaPortPairsWithinKmRule:"
                                    << "globalLink created:" << g.fromTerminalId
                                    << "->" << g.toTerminalId
                                    << "distance=" << d << "km";
            }
        }
        return out;
    };
}

static GlobalRuleRegistrar s_seaPortPairsWithinKm(
    "sea_port_pairs_within_km", seaPortPairsWithinKmRule());

} // namespace

QStringList ScenarioLinker::linkageRuleNames()    { return linkageRegistry().keys();    }
QStringList ScenarioLinker::connectionRuleNames() { return connectionRegistry().keys(); }
QStringList ScenarioLinker::globalRuleNames()     { return globalRegistry().keys();     }

namespace
{

bool isExcluded(const QList<NodeLinkage> &all,
                const QString &terminalId,
                const QString &networkName, int nodeId)
{
    for (const NodeLinkage &l : all)
        if (l.excluded && l.terminalId == terminalId &&
            l.networkName == networkName && l.nodeId == nodeId)
            return true;
    return false;
}

bool manualConnectionBelongsToRegion(const ScenarioDocument &doc,
                                      const Connection       &connection,
                                      const QString          &region)
{
    const auto fromIt = doc.terminals.constFind(connection.fromTerminalId);
    if (fromIt == doc.terminals.constEnd())
        return false;

    const auto toIt = doc.terminals.constFind(connection.toTerminalId);
    if (toIt == doc.terminals.constEnd())
        return false;

    if (fromIt->region != region || toIt->region != region)
        return false;

    // Connection.region is a redundant authoring cross-check. Older and
    // hand-authored scenarios may omit it; in that case endpoint ownership is
    // the source of truth. An explicit mismatch remains invalid for this
    // region and is skipped here after validation reports it.
    return connection.region.isEmpty() || connection.region == region;
}

} // namespace

QList<NodeLinkage>
ScenarioLinker::resolveLinkages(const ScenarioDocument &doc,
                                const ScenarioRegistry &registry)
{
    qCInfo(lcScenario) << "ScenarioLinker::resolveLinkages: begin,"
                       << doc.regions.size() << "regions";
    QList<NodeLinkage> out;
    for (auto it = doc.regions.constBegin(); it != doc.regions.constEnd(); ++it)
    {
        const QString &region = it.key();
        const RegionSpec &r   = it.value();

        QList<NodeLinkage> manual;
        for (const NodeLinkage &l : doc.linkages)
            if (!l.excluded && doc.terminals.contains(l.terminalId) &&
                doc.terminals[l.terminalId].region == region)
                manual.append(l);

        QList<NodeLinkage> autoFromRules;
        for (const QString &ruleName : r.linkageAutoRules)
        {
            auto fn = linkageRegistry().value(ruleName);
            if (fn) autoFromRules.append(fn(doc, registry, region));
        }

        qCDebug(lcScenario) << "ScenarioLinker::resolveLinkages: region"
                            << region << "manual=" << manual.size()
                            << "auto=" << autoFromRules.size()
                            << "strategy=" << linkageStrategyToString(r.linkageStrategy);

        switch (r.linkageStrategy)
        {
        case LinkageStrategy::Manual:
            out.append(manual);
            break;
        case LinkageStrategy::Auto:
            for (const NodeLinkage &l : autoFromRules)
                if (!isExcluded(doc.linkages, l.terminalId, l.networkName, l.nodeId))
                    out.append(l);
            break;
        case LinkageStrategy::Hybrid:
            out.append(manual);
            for (const NodeLinkage &l : autoFromRules)
                if (!isExcluded(doc.linkages, l.terminalId, l.networkName, l.nodeId))
                    out.append(l);
            break;
        }
    }
    qCDebug(lcScenario) << "ScenarioLinker::resolveLinkages: total resolved"
                        << out.size();
    if (out.isEmpty())
        qCWarning(lcScenario) << "ScenarioLinker::resolveLinkages: zero linkages resolved";
    return out;
}

QList<Connection>
ScenarioLinker::resolveConnections(const ScenarioDocument &doc,
                                   const ScenarioRegistry &registry)
{
    qCInfo(lcScenario) << "ScenarioLinker::resolveConnections: begin,"
                       << doc.regions.size() << "regions";
    QList<Connection> out;
    for (auto it = doc.regions.constBegin(); it != doc.regions.constEnd(); ++it)
    {
        const QString &region = it.key();
        const RegionSpec &r   = it.value();

        // Manual set = connections whose region matches AND whose endpoints
        // both belong to this region (defence against stale/malformed entries).
        QList<Connection> manual;
        for (const Connection &c : doc.connections)
        {
            if (manualConnectionBelongsToRegion(doc, c, region))
                manual.append(c);
        }

        QList<Connection> autoFromRules;
        for (const QString &ruleName : r.connectionAutoRules)
        {
            auto fn = connectionRegistry().value(ruleName);
            if (fn) autoFromRules.append(fn(doc, registry, region));
        }

        qCDebug(lcScenario) << "ScenarioLinker::resolveConnections: region"
                            << region << "manual=" << manual.size()
                            << "auto=" << autoFromRules.size();

        // Connection has no explicit `excluded` field; the three strategies
        // below are union/verbatim/rule-only — no filtering.
        switch (r.connectionStrategy)
        {
        case LinkageStrategy::Manual: out.append(manual); break;
        case LinkageStrategy::Auto:   out.append(autoFromRules); break;
        case LinkageStrategy::Hybrid: out.append(manual); out.append(autoFromRules); break;
        }
    }
    qCDebug(lcScenario) << "ScenarioLinker::resolveConnections: total resolved"
                        << out.size();
    return out;
}

QList<GlobalLink>
ScenarioLinker::resolveGlobalLinks(const ScenarioDocument &doc,
                                   const ScenarioRegistry &registry)
{
    qCInfo(lcScenario) << "ScenarioLinker::resolveGlobalLinks: begin,"
                       << "manual=" << doc.globalLinks.size()
                       << "autoRules=" << doc.globalLinkAutoRules.size();
    QList<GlobalLink> out;

    // Manual set = the document's globalLinks as-is. Endpoints may be bare
    // ids or qualified "region/id" strings; validation owns cross-form
    // consistency checks.
    const QList<GlobalLink> &manual = doc.globalLinks;

    QList<GlobalLink> autoFromRules;
    for (const QString &ruleName : doc.globalLinkAutoRules)
    {
        auto fn = globalRegistry().value(ruleName);
        if (!fn) continue;
        const QVariant param = doc.globalLinkAutoRuleParams.value(ruleName);
        autoFromRules.append(fn(doc, registry, param));
    }

    qCDebug(lcScenario) << "ScenarioLinker::resolveGlobalLinks:"
                        << "manual=" << manual.size()
                        << "autoGenerated=" << autoFromRules.size();

    // GlobalLink has no `excluded` field — mirror connection semantics.
    switch (doc.globalLinkStrategy)
    {
    case LinkageStrategy::Manual: out.append(manual); break;
    case LinkageStrategy::Auto:   out.append(autoFromRules); break;
    case LinkageStrategy::Hybrid: out.append(manual); out.append(autoFromRules); break;
    }
    qCDebug(lcScenario) << "ScenarioLinker::resolveGlobalLinks: total resolved"
                        << out.size();
    return out;
}

bool ScenarioLinker::loadNetworksForPreview(const ScenarioDocument &doc,
                                            ScenarioRegistry &registry,
                                            QString *error)
{
    int networkCount = 0;
    for (const auto &r : doc.regions)
        networkCount += r.networks.size();
    qCInfo(lcScenario) << "ScenarioLinker::loadNetworksForPreview: begin,"
                       << networkCount << "networks across"
                       << doc.regions.size() << "regions";
    // Mirrors the production load path in RegionData::addTrainNetwork
    // (RegionDataController.cpp:62-103) and addTruckNetwork (.cpp:106-),
    // but skips NetworkController registration so the live simulation
    // state is not mutated.
    //
    // Rail: construct network, then loadNetwork(nodesFile, linksFile).
    // Truck: readConfig(configFile) returns an IntegrationSimulationConfig
    //        that owns a populated IntegrationNetwork. Store the config.

    for (const RegionSpec &r : doc.regions.values())
    {
        for (const NetworkSpec &n : r.networks.values())
        {
            try
            {
                if (n.type == NetworkSpec::Type::Rail)
                {
                    const QString nodesFile = n.files.value("nodes");
                    const QString linksFile = n.files.value("links");
                    if (nodesFile.isEmpty() || linksFile.isEmpty())
                        throw std::runtime_error(
                            "Rail network requires 'nodes' and 'links' files");

                    auto *net = new TrainClient::NeTrainSimNetwork();
                    net->loadNetwork(nodesFile, linksFile);
                    registry.setPreviewRailNetwork(n.name, net);
                    qCDebug(lcScenario) << "ScenarioLinker::loadNetworksForPreview:"
                                        << "loaded rail network" << n.name;
                }
                else if (n.type == NetworkSpec::Type::Truck)
                {
                    const QString configFile = n.files.value("config");
                    if (configFile.isEmpty())
                        throw std::runtime_error(
                            "Truck network requires 'config' file");

                    TruckClient::IntegrationSimulationConfig *cfg =
                        TruckClient::IntegrationSimulationConfigReader::readConfig(configFile);
                    if (!cfg)
                        throw std::runtime_error(
                            "IntegrationSimulationConfigReader returned null");

                    registry.setPreviewTruckConfig(n.name, cfg);
                    qCDebug(lcScenario) << "ScenarioLinker::loadNetworksForPreview:"
                                        << "loaded truck network" << n.name;
                }
            }
            catch (const std::exception &e)
            {
                qCWarning(lcScenario) << "ScenarioLinker::loadNetworksForPreview:"
                                      << "failed for network" << n.name << "-" << e.what();
                if (error)
                    *error = QStringLiteral("Preview load failed for network '%1': %2")
                                 .arg(n.name, QString::fromUtf8(e.what()));
                return false;
            }
        }
    }
    qCInfo(lcScenario) << "ScenarioLinker::loadNetworksForPreview: complete";
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
