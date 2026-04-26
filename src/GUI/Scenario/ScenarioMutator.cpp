#include "ScenarioMutator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/LatLon.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"

#include <QUuid>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

/// QPointF-to-LatLon convention bridge. Qt uses (x=longitude, y=latitude)
/// for geo points in QPointF; LatLon stores them as named fields. Kept
/// file-local so the convention lives in one spot.
Backend::Scenario::LatLon toLatLon(const QPointF &p)
{
    return Backend::Scenario::LatLon{ p.y(), p.x() };
}

/// DRY: shared by createTerminal and setTerminalRole.
void applyRoleSideEffects(
    CargoNetSim::Backend::Scenario::TerminalPlacement &p)
{
    using Role =
        CargoNetSim::Backend::Scenario::TerminalPlacement::TerminalRole;
    if (p.role == Role::Origin)
    {
        if (p.properties
                .value(PK::Terminal::InitialContainerCount, 0)
                .toInt()
            == 0)
            p.properties[
                PK::Terminal::InitialContainerCount] = 100;
    }
    else if (p.role == Role::Transit)
    {
        p.properties.remove(
            PK::Terminal::InitialContainerCount);
        p.properties.remove(
            PK::Terminal::DestinationTerminal);
        p.properties.remove(PK::Terminal::Destinations);
    }
}

} // namespace

QString ScenarioMutator::createTerminal(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalType,
    const QString                       &region,
    const QPointF                       &localLatLon,
    Backend::Scenario::TerminalPlacement::TerminalRole role)
{
    using namespace Backend::Scenario;
    if (!doc) return QString();
    qCInfo(lcGuiScene) << "ScenarioMutator::createTerminal:"
                       << "type=" << terminalType
                       << "region=" << region
                       << "role=" << roleToString(role);

    TerminalPlacement p;
    p.id = QStringLiteral("%1-%2")
               .arg(terminalType,
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
    p.type       = terminalType;
    p.region     = region;
    p.mode       = TerminalPlacement::PositionMode::LatLon;
    p.latLon     = toLatLon(localLatLon);
    p.properties = TerminalTypeDefaults::defaultProperties(terminalType);
    p.role       = role;
    applyRoleSideEffects(p);

    if (!doc->addTerminal(p)) return QString();
    return p.id;
}

bool ScenarioMutator::removeTerminal(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId)
{
    if (!doc) return false;
    qCInfo(lcGuiScene) << "ScenarioMutator::removeTerminal:"
                       << "id=" << terminalId;
    return doc->removeTerminal(terminalId);
}

bool ScenarioMutator::updateTerminalPosition(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    const QPointF                       &localLatLon)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->terminals.contains(terminalId)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::updateTerminalPosition:"
                        << "id=" << terminalId;

    TerminalPlacement p = doc->terminals[terminalId];
    p.mode   = TerminalPlacement::PositionMode::LatLon;
    p.latLon = toLatLon(localLatLon);
    return doc->updateTerminal(terminalId, p);
}

bool ScenarioMutator::setTerminalProperty(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    const QString                       &key,
    const QVariant                      &value)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->terminals.contains(terminalId)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::setTerminalProperty:"
                        << "id=" << terminalId
                        << "key=" << key << "value=" << value;

    TerminalPlacement p = doc->terminals[terminalId];
    if (value.isValid()) p.properties.insert(key, value);
    else                 p.properties.remove(key);
    return doc->updateTerminal(terminalId, p);
}

bool ScenarioMutator::setTerminalRole(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    Backend::Scenario::TerminalPlacement::TerminalRole role)
{
    if (!doc || !doc->terminals.contains(terminalId))
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::setTerminalRole:"
            << "doc null or terminal not found:"
            << terminalId;
        return false;
    }

    qCInfo(lcGuiScene)
        << "ScenarioMutator::setTerminalRole:"
        << terminalId
        << "role="
        << Backend::Scenario::roleToString(role);

    Backend::Scenario::TerminalPlacement p =
        doc->terminals[terminalId];
    p.role = role;
    applyRoleSideEffects(p);

    return doc->updateTerminal(terminalId, p);
}

bool ScenarioMutator::setTerminalType(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    const QString                       &newType)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::setTerminalType:"
            << "doc null or terminal not found:" << terminalId;
        return false;
    }
    if (!TerminalTypeDefaults::isValidType(newType))
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::setTerminalType:"
            << "invalid type:" << newType;
        return false;
    }

    TerminalPlacement p = doc->terminals[terminalId];
    if (p.type == newType) return true;

    qCInfo(lcGuiScene)
        << "ScenarioMutator::setTerminalType:"
        << terminalId << p.type << "->" << newType;

    p.type       = newType;
    p.properties = TerminalTypeDefaults::defaultProperties(newType);
    applyRoleSideEffects(p);
    p.interfaces = {};  // clear override; runtime derives from new type

    return doc->updateTerminal(terminalId, p);
}

bool ScenarioMutator::linkTerminalToNode(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    const QString                       &networkName,
    int                                  nodeId,
    Backend::Scenario::LinkageSource     source)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->terminals.contains(terminalId)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::linkTerminalToNode:"
                        << "terminal=" << terminalId
                        << "network=" << networkName
                        << "node=" << nodeId;

    NodeLinkage l;
    l.terminalId  = terminalId;
    l.networkName = networkName;
    l.nodeId      = nodeId;
    l.source      = source;
    l.excluded    = false;
    return doc->addLinkage(l);
}

bool ScenarioMutator::unlinkTerminalFromNode(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &terminalId,
    const QString                       &networkName,
    int                                  nodeId)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::unlinkTerminalFromNode:"
                        << "terminal=" << terminalId
                        << "network=" << networkName
                        << "node=" << nodeId;
    return doc->removeLinkage(terminalId, networkName, nodeId);
}

bool ScenarioMutator::createConnection(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &fromId,
    const QString                       &toId,
    Backend::TransportationTypes::TransportationMode mode)
{
    using namespace Backend::Scenario;
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::createConnection:"
                        << "from=" << fromId << "to=" << toId
                        << "mode=" << static_cast<int>(mode);
    if (!doc->terminals.contains(fromId) || !doc->terminals.contains(toId))
        return false;

    const QString fromRegion = doc->terminals[fromId].region;
    const QString toRegion   = doc->terminals[toId].region;
    if (fromRegion != toRegion) return false;

    Connection c;
    c.fromTerminalId = fromId;
    c.toTerminalId   = toId;
    c.mode           = mode;
    c.region         = fromRegion;  // invariant: matches both endpoints
    c.properties     =
        Backend::Scenario::RouteMetricUnits::
            defaultCanonicalProperties();
    c.source         = LinkageSource::Manual;
    return doc->addConnection(c);
}

bool ScenarioMutator::removeConnection(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &fromId,
    const QString                       &toId,
    Backend::TransportationTypes::TransportationMode mode)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::removeConnection:"
                        << "from=" << fromId << "to=" << toId
                        << "mode=" << static_cast<int>(mode);
    return doc->removeConnection(fromId, toId, mode);
}

bool ScenarioMutator::createGlobalLink(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &fromId,
    const QString                       &toId,
    Backend::TransportationTypes::TransportationMode mode)
{
    using namespace Backend::Scenario;
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::createGlobalLink:"
                        << "from=" << fromId << "to=" << toId
                        << "mode=" << static_cast<int>(mode);
    if (!doc->terminals.contains(fromId) || !doc->terminals.contains(toId))
        return false;

    // A GlobalLink spans regions by definition — a same-region pair is
    // expressed as a region-local Connection, not a GlobalLink. Symmetric
    // to createConnection's cross-region rejection: each entity has its
    // own domain and the mutator enforces the boundary at a single point.
    const QString fromRegion = doc->terminals[fromId].region;
    const QString toRegion   = doc->terminals[toId].region;
    if (fromRegion == toRegion)
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::createGlobalLink:"
            << "rejected same-region pair"
            << fromId << "(region=" << fromRegion << ") ->"
            << toId   << "(region=" << toRegion   << ")"
            << "- use createConnection for intra-region links.";
        return false;
    }

    GlobalLink g;
    g.fromTerminalId = fromId;
    g.toTerminalId   = toId;
    g.mode           = mode;
    g.properties     =
        Backend::Scenario::RouteMetricUnits::
            defaultCanonicalProperties();
    g.source         = LinkageSource::Manual;
    return doc->addGlobalLink(g);
}

bool ScenarioMutator::removeGlobalLink(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &fromId,
    const QString                       &toId,
    Backend::TransportationTypes::TransportationMode mode)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::removeGlobalLink:"
                        << "from=" << fromId << "to=" << toId
                        << "mode=" << static_cast<int>(mode);
    return doc->removeGlobalLink(fromId, toId, mode);
}

bool ScenarioMutator::setConnectionProperty(
    Backend::Scenario::ScenarioDocument *doc,
    const QString &fromId, const QString &toId,
    Backend::TransportationTypes::TransportationMode mode,
    const QString &key, const QVariant &value)
{
    if (!doc)
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::setConnectionProperty:"
            << "doc null";
        return false;
    }

    qCDebug(lcGuiScene)
        << "ScenarioMutator::setConnectionProperty:"
        << fromId << "->" << toId
        << "key=" << key << "value=" << value;

    // Search regional connections
    for (auto &c : doc->connections)
    {
        if (c.fromTerminalId == fromId
            && c.toTerminalId == toId
            && c.mode == mode)
        {
            c.properties[key] = value;
            return doc->updateConnection(fromId, toId, mode, c);
        }
    }
    // Search global links
    for (auto &g : doc->globalLinks)
    {
        if (g.fromTerminalId == fromId
            && g.toTerminalId == toId
            && g.mode == mode)
        {
            g.properties[key] = value;
            return doc->updateGlobalLink(fromId, toId, mode, g);
        }
    }

    qCWarning(lcGuiScene)
        << "ScenarioMutator::setConnectionProperty:"
        << "connection not found";
    return false;
}

bool ScenarioMutator::updateRegionLocalOrigin(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &regionName,
    const QPointF                       &latLon)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->regions.contains(regionName)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::updateRegionLocalOrigin:"
                        << "region=" << regionName;

    RegionSpec r = doc->regions[regionName];
    r.localOrigin = toLatLon(latLon);
    return doc->updateRegion(regionName, r);
}

bool ScenarioMutator::updateRegionGlobalPosition(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &regionName,
    const QPointF                       &latLon)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->regions.contains(regionName)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::updateRegionGlobalPosition:"
                        << "region=" << regionName;

    RegionSpec r = doc->regions[regionName];
    r.globalPosition = toLatLon(latLon);
    return doc->updateRegion(regionName, r);
}

bool ScenarioMutator::addRegion(
    Backend::Scenario::ScenarioDocument     *doc,
    const Backend::Scenario::RegionSpec     &spec)
{
    if (!doc || spec.name.isEmpty()) return false;
    qCInfo(lcGuiScene) << "ScenarioMutator::addRegion:" << spec.name;
    return doc->addRegion(spec);
}

bool ScenarioMutator::removeRegion(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &name)
{
    if (!doc || !doc->regions.contains(name)) return false;
    qCInfo(lcGuiScene) << "ScenarioMutator::removeRegion:" << name;
    return doc->removeRegion(name);
}

bool ScenarioMutator::updateRegionColor(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &name,
    const QString                       &colorHex)
{
    using namespace Backend::Scenario;
    if (!doc || !doc->regions.contains(name)) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::updateRegionColor:"
                        << name << colorHex;
    RegionSpec r = doc->regions[name];
    r.color      = colorHex;
    return doc->updateRegion(name, r);
}

bool ScenarioMutator::addNetwork(
    Backend::Scenario::ScenarioDocument        *doc,
    const QString                              &region,
    const Backend::Scenario::NetworkSpec       &spec)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::addNetwork:"
                        << region << spec.name;
    return doc->addNetwork(region, spec);
}

bool ScenarioMutator::removeNetwork(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &region,
    const QString                       &networkName)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::removeNetwork:"
                        << region << networkName;
    return doc->removeNetwork(region, networkName);
}

bool ScenarioMutator::renameNetwork(
    Backend::Scenario::ScenarioDocument *doc,
    const QString                       &region,
    const QString                       &oldName,
    const QString                       &newName)
{
    if (!doc) return false;
    qCDebug(lcGuiScene) << "ScenarioMutator::renameNetwork:"
                        << region << oldName << "->" << newName;
    return doc->renameNetwork(region, oldName, newName);
}

bool ScenarioMutator::updateSimulationSettings(
    Backend::Scenario::ScenarioDocument         *doc,
    const Backend::Scenario::SimulationSettings &settings)
{
    if (!doc) return false;
    const auto timeStep = settings.timeStepUnits();
    qCInfo(lcGuiScene) << "ScenarioMutator::updateSimulationSettings"
                       << "timeStep="
                       << (timeStep.has_value()
                               ? timeStep->value()
                               : -1.0);
    doc->simulation = settings;
    return true;
}

bool ScenarioMutator::updateFleet(
    Backend::Scenario::ScenarioDocument *doc,
    const Backend::Scenario::FleetSpec  &fleet)
{
    if (!doc) return false;
    qCInfo(lcGuiScene) << "ScenarioMutator::updateFleet"
                       << "trainFiles=" << fleet.trainsFiles.size()
                       << "shipFiles="  << fleet.shipsFiles.size();
    doc->fleet = fleet;
    return true;
}

// ---------------- undo-restore helpers ----------------

bool ScenarioMutator::restoreConnection(
    Backend::Scenario::ScenarioDocument   *doc,
    const Backend::Scenario::Connection   &snapshot)
{
    if (!doc) return false;
    for (const auto &c : doc->connections)
    {
        if (c.fromTerminalId == snapshot.fromTerminalId
         && c.toTerminalId   == snapshot.toTerminalId
         && c.mode           == snapshot.mode)
        {
            qCWarning(lcGuiScene)
                << "ScenarioMutator::restoreConnection:"
                << "duplicate (from,to,mode); skipping";
            return false;
        }
    }
    qCInfo(lcGuiScene) << "ScenarioMutator::restoreConnection:"
                       << "from=" << snapshot.fromTerminalId
                       << "to="   << snapshot.toTerminalId;
    return doc->addConnection(snapshot);
}

bool ScenarioMutator::restoreGlobalLink(
    Backend::Scenario::ScenarioDocument   *doc,
    const Backend::Scenario::GlobalLink   &snapshot)
{
    if (!doc) return false;
    for (const auto &g : doc->globalLinks)
    {
        if (g.fromTerminalId == snapshot.fromTerminalId
         && g.toTerminalId   == snapshot.toTerminalId
         && g.mode           == snapshot.mode)
        {
            qCWarning(lcGuiScene)
                << "ScenarioMutator::restoreGlobalLink:"
                << "duplicate (from,to,mode); skipping";
            return false;
        }
    }
    qCInfo(lcGuiScene) << "ScenarioMutator::restoreGlobalLink:"
                       << "from=" << snapshot.fromTerminalId
                       << "to="   << snapshot.toTerminalId;
    return doc->addGlobalLink(snapshot);
}

bool ScenarioMutator::restoreLinkage(
    Backend::Scenario::ScenarioDocument   *doc,
    const Backend::Scenario::NodeLinkage  &snapshot)
{
    if (!doc) return false;
    for (const auto &l : doc->linkages)
    {
        if (l.terminalId  == snapshot.terminalId
         && l.networkName == snapshot.networkName
         && l.nodeId      == snapshot.nodeId)
        {
            qCWarning(lcGuiScene)
                << "ScenarioMutator::restoreLinkage:"
                << "duplicate (terminal,network,node); skipping";
            return false;
        }
    }
    qCInfo(lcGuiScene) << "ScenarioMutator::restoreLinkage:"
                       << "terminal=" << snapshot.terminalId
                       << "network="  << snapshot.networkName
                       << "node="     << snapshot.nodeId;
    return doc->addLinkage(snapshot);
}

bool ScenarioMutator::restoreTerminal(
    Backend::Scenario::ScenarioDocument        *doc,
    const Backend::Scenario::TerminalPlacement &snapshot)
{
    if (!doc) return false;
    if (snapshot.id.isEmpty()) return false;
    if (doc->terminals.contains(snapshot.id))
    {
        qCWarning(lcGuiScene)
            << "ScenarioMutator::restoreTerminal:"
            << "duplicate id; skipping" << snapshot.id;
        return false;
    }
    qCInfo(lcGuiScene) << "ScenarioMutator::restoreTerminal:"
                       << "id=" << snapshot.id
                       << "type=" << snapshot.type;
    return doc->addTerminal(snapshot);
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
