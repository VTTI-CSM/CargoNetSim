#include "ScenarioLinker.h"

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/GeoDistance.h"
#include "Backend/Commons/GeoProjection.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "InterfaceConversion.h"
#include "NetworkLookup.h"
#include "PathMetricsCalculator.h"
#include "PropertyKeys.h"
#include "RouteMetricUnits.h"
#include "ScenarioEndpointResolver.h"
#include "TerminalTypeDefaults.h"

#include <QMap>
#include <QPair>
#include <QPointF>
#include <QSet>
#include <QtMath>
#include <cmath>
#include <optional>

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

QPair<double, double> projectedTrainNode(
    const TrainClient::NeTrainSimNode &node)
{
    return { node.getX() * node.getXScale(),
             node.getY() * node.getYScale() };
}

QPair<double, double> projectedTruckNode(
    const TruckClient::IntegrationNode &node)
{
    return {
        Units::toMeters(
            Units::kilometers(node.getXCoordinate()
                              * node.getXScale()))
            .value(),
        Units::toMeters(
            Units::kilometers(node.getYCoordinate()
                              * node.getYScale()))
            .value()
    };
}

QPair<double, double> projectedLatLon(
    const TerminalPlacement &terminal)
{
    const QPointF point =
        Commons::GeoProjection::wgs84ToWebMercatorMeters(
            QPointF(terminal.latLon.longitude,
                    terminal.latLon.latitude));
    return { point.x(), point.y() };
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
                    return projectedTrainNode(*n);
        }
        return { 0.0, 0.0 };
    case TerminalPlacement::PositionMode::LatLon:
        return projectedLatLon(t);
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
                    const auto nodeCoords = projectedTrainNode(*n);
                    const double d = sceneDistance(coords.first, coords.second,
                                                   nodeCoords.first,
                                                   nodeCoords.second);
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
                    return projectedTruckNode(*n);
        }
        return { 0.0, 0.0 };
    case TerminalPlacement::PositionMode::LatLon:
        return projectedLatLon(t);
    }
    return { 0.0, 0.0 };
}

namespace PK = PropertyKeys;
using Mode = TransportationTypes::TransportationMode;

bool isPositiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

std::optional<Mode> transportationModeForNetworkType(NetworkSpec::Type type)
{
    switch (type)
    {
    case NetworkSpec::Type::Rail:
        return Mode::Train;
    case NetworkSpec::Type::Truck:
        return Mode::Truck;
    }
    return std::nullopt;
}

bool hasCompleteRouteMetrics(const QVariantMap &properties)
{
    return RouteMetricUnits::missingCanonicalRouteMetricKeys(
        properties).isEmpty();
}

void mergeMissingRouteMetrics(QVariantMap       &properties,
                              const QVariantMap &computedProperties)
{
    for (const QString &key : RouteMetricUnits::routeMetricKeys())
    {
        bool ok = false;
        properties.value(key).toDouble(&ok);
        if (!properties.contains(key) || !ok)
            properties.insert(key, computedProperties.value(key));
    }
}

std::optional<QPointF> terminalGlobalPositionForRoute(
    const ScenarioDocument &doc,
    const QString          &terminalId)
{
    const auto endpoint = resolveTerminalEndpoint(doc, terminalId);
    if (!endpoint.found || !endpoint.terminal)
        return std::nullopt;

    const QPointF stored = doc.globalPositionOf(endpoint.terminalId);
    if (std::isfinite(stored.x()) && std::isfinite(stored.y()))
        return stored;

    const auto regionIt = doc.regions.constFind(endpoint.region);
    if (regionIt == doc.regions.constEnd())
        return std::nullopt;

    const double lon = regionIt->globalPosition.longitude;
    const double lat = regionIt->globalPosition.latitude;
    if (!std::isfinite(lon) || !std::isfinite(lat))
        return std::nullopt;

    qCDebug(lcScenario)
        << "ScenarioLinker: using region global position as"
           " approximate route coordinate for terminal"
        << endpoint.terminalId;
    return QPointF(lon, lat);
}

std::optional<QVariantMap> canonicalPropertiesFromEstimate(
    const ScenarioDocument &doc,
    Mode                    mode,
    double                  distanceMeters,
    double                  travelTimeSeconds,
    bool                    useNetworkTravelTime)
{
    if (!isPositiveFinite(distanceMeters))
        return std::nullopt;

    auto *controller = CargoNetSim::CargoNetSimController::instance();
    auto *config = controller ? controller->getConfigController()
                              : nullptr;
    if (!config)
    {
        qCWarning(lcScenario)
            << "ScenarioLinker: cannot compute route metrics;"
            << "ConfigController unavailable";
        return std::nullopt;
    }

    auto inputs =
        PathMetricsCalculator::gatherInputs(mode, *config,
                                            doc.simulation);
    if (inputs.modeProperties.isEmpty())
        return std::nullopt;

    if (!useNetworkTravelTime
        || !isPositiveFinite(travelTimeSeconds))
    {
        inputs.modeProperties[PK::Mode::UseNetwork] = false;
        travelTimeSeconds = 0.0;
    }

    const PathMetrics metrics =
        PathMetricsCalculator::compute(distanceMeters,
                                       travelTimeSeconds,
                                       mode,
                                       inputs);
    if (!metrics.valid
        || !isPositiveFinite(metrics.distanceKm)
        || !std::isfinite(metrics.travelTimeHours))
    {
        return std::nullopt;
    }

    return RouteMetricUnits::canonicalPropertiesFromMetrics(metrics);
}

std::optional<QVariantMap> approximatePropertiesFromTerminalPositions(
    const ScenarioDocument &doc,
    const QString          &fromTerminalId,
    const QString          &toTerminalId,
    Mode                    mode)
{
    const auto from = terminalGlobalPositionForRoute(doc, fromTerminalId);
    const auto to = terminalGlobalPositionForRoute(doc, toTerminalId);
    if (!from || !to)
        return std::nullopt;

    const double distanceMeters =
        Commons::GeoDistance::haversineMeters(
            from->y(), from->x(), to->y(), to->x());
    return canonicalPropertiesFromEstimate(
        doc, mode, distanceMeters, 0.0, false);
}

std::optional<QVariantMap> networkBackedPropertiesForConnection(
    const ScenarioDocument &doc,
    const ScenarioRegistry &registry,
    const Connection       &connection,
    NetworkSpec::Type       networkType)
{
    const auto fromIt =
        doc.terminals.constFind(connection.fromTerminalId);
    const auto toIt =
        doc.terminals.constFind(connection.toTerminalId);
    if (fromIt == doc.terminals.constEnd()
        || toIt == doc.terminals.constEnd())
    {
        return std::nullopt;
    }

    const auto fromLinks =
        doc.linkagesFor(connection.fromTerminalId, networkType);
    const auto toLinks =
        doc.linkagesFor(connection.toTerminalId, networkType);

    for (const NodeLinkage &fromLink : fromLinks)
    {
        for (const NodeLinkage &toLink : toLinks)
        {
            if (fromLink.networkName != toLink.networkName)
                continue;

            ShortestPathResult path;
            if (networkType == NetworkSpec::Type::Rail)
            {
                auto *network = NetworkLookup::findRail(
                    registry, fromIt->region, fromLink.networkName);
                if (!network)
                    continue;
                path = network->findShortestPath(
                    fromLink.nodeId, toLink.nodeId,
                    QStringLiteral("distance"));
            }
            else
            {
                auto *network = NetworkLookup::findTruck(
                    registry, fromIt->region, fromLink.networkName);
                if (!network)
                    continue;
                path = network->findShortestPath(
                    fromLink.nodeId, toLink.nodeId);
            }

            auto properties = canonicalPropertiesFromEstimate(
                doc, connection.mode, path.totalLength,
                path.minTravelTime, true);
            if (properties)
                return properties;
        }
    }

    return std::nullopt;
}

void enrichConnectionRouteMetrics(Connection             &connection,
                                  const ScenarioDocument &doc,
                                  const ScenarioRegistry &registry)
{
    if (hasCompleteRouteMetrics(connection.properties))
        return;

    std::optional<QVariantMap> computedProperties;
    if (connection.mode == Mode::Train)
    {
        computedProperties =
            networkBackedPropertiesForConnection(
                doc, registry, connection, NetworkSpec::Type::Rail);
    }
    else if (connection.mode == Mode::Truck)
    {
        computedProperties =
            networkBackedPropertiesForConnection(
                doc, registry, connection, NetworkSpec::Type::Truck);
    }

    if (!computedProperties)
    {
        computedProperties =
            approximatePropertiesFromTerminalPositions(
                doc, connection.fromTerminalId,
                connection.toTerminalId, connection.mode);
    }

    if (!computedProperties)
    {
        qCWarning(lcScenario)
            << "ScenarioLinker: route metrics unavailable for connection"
            << connection.fromTerminalId << "->"
            << connection.toTerminalId
            << "mode=" << static_cast<int>(connection.mode);
        return;
    }

    mergeMissingRouteMetrics(connection.properties,
                             computedProperties.value());
}

void enrichGlobalLinkRouteMetrics(GlobalLink             &globalLink,
                                  const ScenarioDocument &doc)
{
    if (hasCompleteRouteMetrics(globalLink.properties))
        return;

    const auto fromEndpoint =
        resolveTerminalEndpoint(doc, globalLink.fromTerminalId);
    const auto toEndpoint =
        resolveTerminalEndpoint(doc, globalLink.toTerminalId);
    if (!fromEndpoint.found || !toEndpoint.found)
        return;

    const auto computedProperties =
        approximatePropertiesFromTerminalPositions(
            doc, fromEndpoint.terminalId,
            toEndpoint.terminalId, globalLink.mode);
    if (!computedProperties)
    {
        qCWarning(lcScenario)
            << "ScenarioLinker: route metrics unavailable for global link"
            << globalLink.fromTerminalId << "->"
            << globalLink.toTerminalId
            << "mode=" << static_cast<int>(globalLink.mode);
        return;
    }

    mergeMissingRouteMetrics(globalLink.properties,
                             computedProperties.value());
}

void enrichConnectionRouteMetrics(QList<Connection>      &connections,
                                  const ScenarioDocument &doc,
                                  const ScenarioRegistry &registry)
{
    for (Connection &connection : connections)
        enrichConnectionRouteMetrics(connection, doc, registry);
}

void enrichGlobalLinkRouteMetrics(QList<GlobalLink>      &globalLinks,
                                  const ScenarioDocument &doc)
{
    for (GlobalLink &globalLink : globalLinks)
        enrichGlobalLinkRouteMetrics(globalLink, doc);
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
                    const auto nodeCoords = projectedTruckNode(*n);
                    const double d = sceneDistance(coords.first, coords.second,
                                                   nodeCoords.first,
                                                   nodeCoords.second);
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
                    const auto nodeCoords = projectedTrainNode(*n);
                    const double d = sceneDistance(coords.first, coords.second,
                                                   nodeCoords.first,
                                                   nodeCoords.second);
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

                const auto regionIt = doc.regions.constFind(region);
                if (regionIt == doc.regions.constEnd())
                    continue;

                QSet<int> emittedModeValues;
                for (const QString &networkName : shared)
                {
                    const auto networkIt =
                        regionIt->networks.constFind(networkName);
                    if (networkIt == regionIt->networks.constEnd())
                    {
                        qCWarning(lcScenario)
                            << "byNetworksRule: shared network has no"
                            << "specification in region" << region
                            << "network=" << networkName;
                        continue;
                    }

                    const auto mode =
                        transportationModeForNetworkType(networkIt->type);
                    const int modeValue =
                        mode ? static_cast<int>(*mode) : -1;
                    if (!mode || emittedModeValues.contains(modeValue))
                        continue;

                    emittedModeValues.insert(modeValue);
                    Connection c;
                    c.fromTerminalId = terminalIds[i];
                    c.toTerminalId   = terminalIds[j];
                    c.mode           = *mode;
                    c.region         = region;
                    c.source         = LinkageSource::Auto;
                    out.append(c);
                    qCDebug(lcScenario)
                        << "byNetworksRule: connection created:"
                        << terminalIds[i] << "->" << terminalIds[j]
                        << "network=" << networkName
                        << "mode=" << static_cast<int>(*mode)
                        << "in" << region;
                }
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

        auto latLonOf =
            [&](const TerminalPlacement &t) -> std::optional<QPair<double, double>>
        {
            if (t.mode == TerminalPlacement::PositionMode::LatLon)
            {
                const QPointF global = doc.globalPositionOf(t.id);
                if (std::isfinite(global.x())
                    && std::isfinite(global.y()))
                {
                    return QPair<double, double>(global.y(),
                                                 global.x());
                }
            }
            if (doc.regions.contains(t.region))
            {
                const auto &r = doc.regions[t.region];
                if (std::isfinite(r.globalPosition.latitude)
                    && std::isfinite(r.globalPosition.longitude))
                {
                    return QPair<double, double>(
                        r.globalPosition.latitude,
                        r.globalPosition.longitude);
                }
            }
            return std::nullopt;
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
                if (!p || !q)
                    continue;
                const double d = CargoNetSim::Backend::Commons::GeoDistance::haversineKm(
                    p->first, p->second, q->first, q->second);
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

QString networkNodeKey(const NodeLinkage &linkage)
{
    return linkage.networkName
        + QLatin1Char('|')
        + QString::number(linkage.nodeId);
}

void appendIfNetworkNodeAvailable(QList<NodeLinkage> &out,
                                  QSet<QString>      &usedNodes,
                                  const NodeLinkage  &linkage,
                                  const QString      &context)
{
    if (linkage.excluded)
        return;

    const QString key = networkNodeKey(linkage);
    if (usedNodes.contains(key))
    {
        qCWarning(lcScenario)
            << "ScenarioLinker::resolveLinkages:"
            << "skipping duplicate active network-node assignment"
            << "context=" << context
            << "terminal=" << linkage.terminalId
            << "network=" << linkage.networkName
            << "node=" << linkage.nodeId;
        return;
    }

    usedNodes.insert(key);
    out.append(linkage);
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
        QSet<QString> usedNodes;

        switch (r.linkageStrategy)
        {
        case LinkageStrategy::Manual:
            for (const NodeLinkage &l : manual)
                appendIfNetworkNodeAvailable(out, usedNodes, l,
                                             QStringLiteral("manual"));
            break;
        case LinkageStrategy::Auto:
            for (const NodeLinkage &l : autoFromRules)
                if (!isExcluded(doc.linkages, l.terminalId, l.networkName, l.nodeId))
                    appendIfNetworkNodeAvailable(out, usedNodes, l,
                                                 QStringLiteral("auto"));
            break;
        case LinkageStrategy::Hybrid:
            for (const NodeLinkage &l : manual)
                appendIfNetworkNodeAvailable(out, usedNodes, l,
                                             QStringLiteral("hybrid/manual"));
            for (const NodeLinkage &l : autoFromRules)
                if (!isExcluded(doc.linkages, l.terminalId, l.networkName, l.nodeId))
                    appendIfNetworkNodeAvailable(out, usedNodes, l,
                                                 QStringLiteral("hybrid/auto"));
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
    enrichConnectionRouteMetrics(out, doc, registry);
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
    enrichGlobalLinkRouteMetrics(out, doc);
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
