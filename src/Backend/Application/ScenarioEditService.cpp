#include "ScenarioEditService.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/LatLon.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"

#include <QUuid>

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

Scenario::LatLon toLatLon(const QPointF &p)
{
    return Scenario::LatLon{p.y(), p.x()};
}

void applyRoleSideEffects(Scenario::TerminalPlacement &p)
{
    using Role = Scenario::TerminalPlacement::TerminalRole;
    if (p.role == Role::Origin)
    {
        if (p.properties
                .value(PK::Terminal::InitialContainerCount, 0)
                .toInt()
            == 0)
        {
            p.properties[PK::Terminal::InitialContainerCount] =
                100;
        }
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

QString ScenarioEditService::createTerminal(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalType,
    const QString              &region,
    const QPointF              &localLatLon,
    Scenario::TerminalPlacement::TerminalRole role)
{
    using namespace Scenario;
    if (!doc)
        return QString();
    qCInfo(lcGuiScene) << "ScenarioEditService::createTerminal:"
                       << "type=" << terminalType
                       << "region=" << region << "role="
                       << roleToString(role);

    TerminalPlacement p;
    p.id = QStringLiteral("%1-%2")
               .arg(terminalType,
                    QUuid::createUuid().toString(
                        QUuid::WithoutBraces));
    p.type = terminalType;
    p.region = region;
    p.mode = TerminalPlacement::PositionMode::LatLon;
    p.latLon = toLatLon(localLatLon);
    p.properties =
        TerminalTypeDefaults::defaultProperties(terminalType);
    p.role = role;
    applyRoleSideEffects(p);

    if (!doc->addTerminal(p))
        return QString();
    return p.id;
}

bool ScenarioEditService::setTerminalRole(
    Scenario::ScenarioDocument             *doc,
    const QString                          &terminalId,
    Scenario::TerminalPlacement::TerminalRole role)
{
    if (!doc || !doc->terminals.contains(terminalId))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setTerminalRole:"
            << "doc null or terminal not found:"
            << terminalId;
        return false;
    }

    qCInfo(lcGuiScene)
        << "ScenarioEditService::setTerminalRole:" << terminalId
        << "role=" << Scenario::roleToString(role);

    Scenario::TerminalPlacement p = doc->terminals[terminalId];
    p.role = role;
    applyRoleSideEffects(p);

    return doc->updateTerminal(terminalId, p);
}

bool ScenarioEditService::setTerminalType(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId,
    const QString              &newType)
{
    using namespace Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setTerminalType:"
            << "doc null or terminal not found:" << terminalId;
        return false;
    }
    if (!TerminalTypeDefaults::isValidType(newType))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setTerminalType:"
            << "invalid type:" << newType;
        return false;
    }

    TerminalPlacement p = doc->terminals[terminalId];
    if (p.type == newType)
        return true;

    qCInfo(lcGuiScene) << "ScenarioEditService::setTerminalType:"
                       << terminalId << p.type << "->"
                       << newType;

    p.type = newType;
    p.properties =
        TerminalTypeDefaults::defaultProperties(newType);
    applyRoleSideEffects(p);
    p.interfaces = {};

    return doc->updateTerminal(terminalId, p);
}

bool ScenarioEditService::removeTerminal(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId)
{
    if (!doc)
        return false;
    qCInfo(lcGuiScene) << "ScenarioEditService::removeTerminal:"
                       << "id=" << terminalId;
    return doc->removeTerminal(terminalId);
}

bool ScenarioEditService::updateTerminalPosition(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId,
    const QPointF              &localLatLon)
{
    using namespace Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::updateTerminalPosition:"
        << "id=" << terminalId;

    TerminalPlacement p = doc->terminals[terminalId];
    p.mode = TerminalPlacement::PositionMode::LatLon;
    p.latLon = toLatLon(localLatLon);
    return doc->updateTerminal(terminalId, p);
}

bool ScenarioEditService::setTerminalProperty(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId,
    const QString              &key,
    const QVariant             &value)
{
    using namespace Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::setTerminalProperty:"
        << "id=" << terminalId << "key=" << key
        << "value=" << value;

    TerminalPlacement p = doc->terminals[terminalId];
    if (value.isValid())
        p.properties.insert(key, value);
    else
        p.properties.remove(key);
    return doc->updateTerminal(terminalId, p);
}

bool ScenarioEditService::linkTerminalToNode(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId,
    const QString              &networkName,
    int                         nodeId,
    Scenario::LinkageSource     source)
{
    using namespace Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::linkTerminalToNode:"
        << "terminal=" << terminalId
        << "network=" << networkName << "node=" << nodeId;

    NodeLinkage l;
    l.terminalId = terminalId;
    l.networkName = networkName;
    l.nodeId = nodeId;
    l.source = source;
    l.excluded = false;
    return doc->addLinkage(l);
}

bool ScenarioEditService::unlinkTerminalFromNode(
    Scenario::ScenarioDocument *doc,
    const QString              &terminalId,
    const QString              &networkName,
    int                         nodeId)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::unlinkTerminalFromNode:"
        << "terminal=" << terminalId
        << "network=" << networkName << "node=" << nodeId;
    return doc->removeLinkage(terminalId, networkName, nodeId);
}

bool ScenarioEditService::createConnection(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromTerminalId,
    const QString                           &toTerminalId,
    TransportationTypes::TransportationMode mode)
{
    return createConnection(
        doc, fromTerminalId, toTerminalId, mode,
        Scenario::RouteMetricUnits::defaultCanonicalProperties(),
        Scenario::LinkageSource::Manual);
}

bool ScenarioEditService::createConnection(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromId,
    const QString                           &toId,
    TransportationTypes::TransportationMode mode,
    const QVariantMap                       &properties,
    Scenario::LinkageSource                  source)
{
    using namespace Scenario;
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::createConnection:"
        << "from=" << fromId << "to=" << toId
        << "mode=" << static_cast<int>(mode);
    if (!doc->terminals.contains(fromId)
        || !doc->terminals.contains(toId))
        return false;

    const QString fromRegion = doc->terminals[fromId].region;
    const QString toRegion = doc->terminals[toId].region;
    if (fromRegion != toRegion)
        return false;

    Connection c;
    c.fromTerminalId = fromId;
    c.toTerminalId = toId;
    c.mode = mode;
    c.region = fromRegion;
    c.properties = properties;
    c.source = source;
    return doc->addConnection(c);
}

bool ScenarioEditService::removeConnection(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromId,
    const QString                           &toId,
    TransportationTypes::TransportationMode mode)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::removeConnection:"
        << "from=" << fromId << "to=" << toId
        << "mode=" << static_cast<int>(mode);
    return doc->removeConnection(fromId, toId, mode);
}

bool ScenarioEditService::createGlobalLink(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromTerminalId,
    const QString                           &toTerminalId,
    TransportationTypes::TransportationMode mode)
{
    return createGlobalLink(
        doc, fromTerminalId, toTerminalId, mode,
        Scenario::RouteMetricUnits::defaultCanonicalProperties(),
        Scenario::LinkageSource::Manual);
}

bool ScenarioEditService::createGlobalLink(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromId,
    const QString                           &toId,
    TransportationTypes::TransportationMode mode,
    const QVariantMap                       &properties,
    Scenario::LinkageSource                  source)
{
    using namespace Scenario;
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::createGlobalLink:"
        << "from=" << fromId << "to=" << toId
        << "mode=" << static_cast<int>(mode);
    if (!doc->terminals.contains(fromId)
        || !doc->terminals.contains(toId))
        return false;

    const QString fromRegion = doc->terminals[fromId].region;
    const QString toRegion = doc->terminals[toId].region;
    if (fromRegion == toRegion)
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::createGlobalLink:"
            << "rejected same-region pair" << fromId
            << "(region=" << fromRegion << ") ->" << toId
            << "(region=" << toRegion << ")"
            << "- use createConnection for intra-region links.";
        return false;
    }

    GlobalLink g;
    g.fromTerminalId = fromId;
    g.toTerminalId = toId;
    g.mode = mode;
    g.properties = properties;
    g.source = source;
    return doc->addGlobalLink(g);
}

bool ScenarioEditService::removeGlobalLink(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromId,
    const QString                           &toId,
    TransportationTypes::TransportationMode mode)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::removeGlobalLink:"
        << "from=" << fromId << "to=" << toId
        << "mode=" << static_cast<int>(mode);
    return doc->removeGlobalLink(fromId, toId, mode);
}

bool ScenarioEditService::setConnectionProperty(
    Scenario::ScenarioDocument              *doc,
    const QString                           &fromId,
    const QString                           &toId,
    TransportationTypes::TransportationMode mode,
    const QString                           &key,
    const QVariant                          &value)
{
    if (!doc)
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setConnectionProperty:"
            << "doc null";
        return false;
    }

    qCDebug(lcGuiScene)
        << "ScenarioEditService::setConnectionProperty:"
        << fromId << "->" << toId << "key=" << key
        << "value=" << value;

    if (auto *c = doc->findConnection(fromId, toId, mode))
    {
        Scenario::Connection updated = *c;
        updated.properties[key] = value;
        return doc->updateConnection(fromId, toId, mode,
                                     updated);
    }
    if (auto *g = doc->findGlobalLink(fromId, toId, mode))
    {
        Scenario::GlobalLink updated = *g;
        updated.properties[key] = value;
        return doc->updateGlobalLink(fromId, toId, mode,
                                     updated);
    }

    qCWarning(lcGuiScene)
        << "ScenarioEditService::setConnectionProperty:"
        << "connection not found";
    return false;
}

bool ScenarioEditService::updateRegionLocalOrigin(
    Scenario::ScenarioDocument *doc,
    const QString              &regionName,
    const QPointF              &latLon)
{
    using namespace Scenario;
    if (!doc || !doc->regions.contains(regionName))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::updateRegionLocalOrigin:"
        << "region=" << regionName;

    RegionSpec r = doc->regions[regionName];
    r.localOrigin = toLatLon(latLon);
    return doc->updateRegion(regionName, r);
}

bool ScenarioEditService::updateRegionGlobalPosition(
    Scenario::ScenarioDocument *doc,
    const QString              &regionName,
    const QPointF              &latLon)
{
    using namespace Scenario;
    if (!doc || !doc->regions.contains(regionName))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::updateRegionGlobalPosition:"
        << "region=" << regionName;

    RegionSpec r = doc->regions[regionName];
    r.globalPosition = toLatLon(latLon);
    return doc->updateRegion(regionName, r);
}

bool ScenarioEditService::addRegion(
    Scenario::ScenarioDocument *doc,
    const Scenario::RegionSpec &spec)
{
    if (!doc || spec.name.isEmpty())
        return false;
    qCInfo(lcGuiScene) << "ScenarioEditService::addRegion:"
                       << spec.name;
    return doc->addRegion(spec);
}

bool ScenarioEditService::removeRegion(
    Scenario::ScenarioDocument *doc,
    const QString              &name)
{
    if (!doc || !doc->regions.contains(name))
        return false;
    qCInfo(lcGuiScene)
        << "ScenarioEditService::removeRegion:" << name;
    return doc->removeRegion(name);
}

bool ScenarioEditService::renameRegion(
    Scenario::ScenarioDocument *doc,
    const QString              &oldName,
    const QString              &newName)
{
    if (!doc)
        return false;
    qCInfo(lcGuiScene)
        << "ScenarioEditService::renameRegion:" << oldName
        << "->" << newName;
    return doc->renameRegion(oldName, newName);
}

bool ScenarioEditService::updateRegionColor(
    Scenario::ScenarioDocument *doc,
    const QString              &name,
    const QString              &colorHex)
{
    using namespace Scenario;
    if (!doc || !doc->regions.contains(name))
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::updateRegionColor:" << name
        << colorHex;
    RegionSpec r = doc->regions[name];
    r.color = colorHex;
    return doc->updateRegion(name, r);
}

bool ScenarioEditService::addNetwork(
    Scenario::ScenarioDocument *doc,
    const QString              &region,
    const Scenario::NetworkSpec &spec)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene) << "ScenarioEditService::addNetwork:"
                        << region << spec.name;
    return doc->addNetwork(region, spec);
}

bool ScenarioEditService::removeNetwork(
    Scenario::ScenarioDocument *doc,
    const QString              &region,
    const QString              &networkName)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::removeNetwork:" << region
        << networkName;
    return doc->removeNetwork(region, networkName);
}

bool ScenarioEditService::renameNetwork(
    Scenario::ScenarioDocument *doc,
    const QString              &region,
    const QString              &oldName,
    const QString              &newName)
{
    if (!doc)
        return false;
    qCDebug(lcGuiScene)
        << "ScenarioEditService::renameNetwork:" << region
        << oldName << "->" << newName;
    return doc->renameNetwork(region, oldName, newName);
}

bool ScenarioEditService::updateSimulationSettings(
    Scenario::ScenarioDocument         *doc,
    const Scenario::SimulationSettings &settings)
{
    if (!doc)
        return false;
    const auto timeStep = settings.timeStepUnits();
    qCInfo(lcGuiScene)
        << "ScenarioEditService::updateSimulationSettings"
        << "timeStep="
        << (timeStep.has_value() ? timeStep->value() : -1.0);
    doc->simulation = settings;
    return true;
}

bool ScenarioEditService::updateFleet(
    Scenario::ScenarioDocument *doc,
    const Scenario::FleetSpec  &fleet)
{
    if (!doc)
        return false;
    qCInfo(lcGuiScene) << "ScenarioEditService::updateFleet"
                       << "trainFiles=" << fleet.trainsFiles.size()
                       << "shipFiles=" << fleet.shipsFiles.size();
    doc->fleet = fleet;
    return true;
}

bool ScenarioEditService::restoreConnection(
    Scenario::ScenarioDocument *doc,
    const Scenario::Connection &snapshot)
{
    if (!doc)
        return false;
    if (doc->findConnection(snapshot.fromTerminalId,
                            snapshot.toTerminalId,
                            snapshot.mode))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::restoreConnection:"
            << "duplicate (from,to,mode); skipping";
        return false;
    }
    qCInfo(lcGuiScene)
        << "ScenarioEditService::restoreConnection:"
        << "from=" << snapshot.fromTerminalId
        << "to=" << snapshot.toTerminalId;
    return doc->addConnection(snapshot);
}

bool ScenarioEditService::restoreGlobalLink(
    Scenario::ScenarioDocument *doc,
    const Scenario::GlobalLink &snapshot)
{
    if (!doc)
        return false;
    if (doc->findGlobalLink(snapshot.fromTerminalId,
                            snapshot.toTerminalId,
                            snapshot.mode))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::restoreGlobalLink:"
            << "duplicate (from,to,mode); skipping";
        return false;
    }
    qCInfo(lcGuiScene)
        << "ScenarioEditService::restoreGlobalLink:"
        << "from=" << snapshot.fromTerminalId
        << "to=" << snapshot.toTerminalId;
    return doc->addGlobalLink(snapshot);
}

bool ScenarioEditService::restoreLinkage(
    Scenario::ScenarioDocument *doc,
    const Scenario::NodeLinkage &snapshot)
{
    if (!doc)
        return false;
    for (const auto &l : doc->linkages)
    {
        if (l.terminalId == snapshot.terminalId
            && l.networkName == snapshot.networkName
            && l.nodeId == snapshot.nodeId)
        {
            qCWarning(lcGuiScene)
                << "ScenarioEditService::restoreLinkage:"
                << "duplicate (terminal,network,node); skipping";
            return false;
        }
    }
    qCInfo(lcGuiScene)
        << "ScenarioEditService::restoreLinkage:"
        << "terminal=" << snapshot.terminalId
        << "network=" << snapshot.networkName
        << "node=" << snapshot.nodeId;
    return doc->addLinkage(snapshot);
}

bool ScenarioEditService::restoreTerminal(
    Scenario::ScenarioDocument         *doc,
    const Scenario::TerminalPlacement &snapshot)
{
    if (!doc)
        return false;
    if (snapshot.id.isEmpty())
        return false;
    if (doc->terminals.contains(snapshot.id))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::restoreTerminal:"
            << "duplicate id; skipping" << snapshot.id;
        return false;
    }
    qCInfo(lcGuiScene)
        << "ScenarioEditService::restoreTerminal:"
        << "id=" << snapshot.id << "type=" << snapshot.type;
    return doc->addTerminal(snapshot);
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
