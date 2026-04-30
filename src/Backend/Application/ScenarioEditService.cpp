#include "ScenarioEditService.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/LatLon.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"

#include <QSet>
#include <QUuid>

#include <cmath>

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

Scenario::TerminalPlacement::InterfaceSet defaultInterfacesForType(
    const QString &terminalType)
{
    Scenario::TerminalPlacement::InterfaceSet interfaces;
    const auto defaults =
        Scenario::TerminalTypeDefaults::interfacesFor(terminalType);
    interfaces.landSide = defaults.first;
    interfaces.seaSide  = defaults.second;
    interfaces.isSet    = true;
    return interfaces;
}

void applyRoleSideEffects(Scenario::TerminalPlacement &p)
{
    using Role = Scenario::TerminalPlacement::TerminalRole;
    if (p.role == Role::Origin)
    {
        if (p.properties
                .value(PK::Terminal::InitialContainerCount, 0)
                .toInt()
            <= 0)
        {
            p.properties[PK::Terminal::InitialContainerCount] =
                100;
        }
    }
    else
    {
        p.properties.remove(
            PK::Terminal::InitialContainerCount);
        p.properties.remove(
            PK::Terminal::DestinationTerminal);
        p.properties.remove(PK::Terminal::Destinations);
    }
}

bool destinationExists(const Scenario::ScenarioDocument *doc,
                       const QString                    &terminalId)
{
    return doc && !terminalId.isEmpty()
           && doc->terminals.contains(terminalId);
}

bool validateDestinationRoutes(const Scenario::ScenarioDocument              *doc,
                               const QString                                &originId,
                               const QList<Scenario::DestinationRoute>      &routes)
{
    if (!destinationExists(doc, originId)
        || !doc->isOrigin(originId)
        || routes.isEmpty())
    {
        return false;
    }

    QSet<QString> seen;
    double        fractionSum = 0.0;
    for (const auto &route : routes)
    {
        if (!destinationExists(doc, route.terminal)
            || route.terminal == originId
            || route.fraction <= 0.0
            || seen.contains(route.terminal))
        {
            return false;
        }
        seen.insert(route.terminal);
        fractionSum += route.fraction;
    }

    return std::abs(fractionSum - 1.0) <= 1e-6;
}

QVariantList destinationRoutesToVariantList(
    const QList<Scenario::DestinationRoute> &routes)
{
    QVariantList out;
    out.reserve(routes.size());
    for (const auto &route : routes)
    {
        QVariantMap row;
        row[PK::Terminal::DestTerminal] = route.terminal;
        row[PK::Terminal::DestFraction] = route.fraction;
        out.append(row);
    }
    return out;
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
    if (!TerminalTypeDefaults::isValidType(terminalType))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::createTerminal:"
            << "invalid terminal type:" << terminalType;
        return QString();
    }

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
    p.interfaces = defaultInterfacesForType(terminalType);
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
    p.interfaces = defaultInterfacesForType(newType);
    applyRoleSideEffects(p);

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

bool ScenarioEditService::updateTerminalPlacement(
    Scenario::ScenarioDocument         *doc,
    const QString                      &terminalId,
    const Scenario::TerminalPlacement &placement)
{
    using namespace Scenario;
    if (!doc || !doc->terminals.contains(terminalId))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::updateTerminalPlacement:"
            << "doc null or terminal not found:" << terminalId;
        return false;
    }
    if (!placement.id.isEmpty() && placement.id != terminalId)
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::updateTerminalPlacement:"
            << "id mismatch:" << terminalId << placement.id;
        return false;
    }
    if (!TerminalTypeDefaults::isValidType(placement.type))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::updateTerminalPlacement:"
            << "invalid terminal type:" << placement.type;
        return false;
    }

    TerminalPlacement sanitized = placement;
    sanitized.id = terminalId;
    sanitized.properties.remove(QStringLiteral("Available Interfaces"));
    applyRoleSideEffects(sanitized);
    return doc->updateTerminal(terminalId, sanitized);
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

bool ScenarioEditService::setOriginDestinationScalar(
    Scenario::ScenarioDocument *doc,
    const QString              &originTerminalId,
    const QString              &destinationTerminalId)
{
    using namespace Scenario;
    if (!destinationExists(doc, originTerminalId)
        || !doc->isOrigin(originTerminalId)
        || !destinationExists(doc, destinationTerminalId)
        || originTerminalId == destinationTerminalId)
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setOriginDestinationScalar:"
            << "invalid origin/destination"
            << originTerminalId << destinationTerminalId;
        return false;
    }

    TerminalPlacement p = doc->terminals[originTerminalId];
    p.properties[PK::Terminal::DestinationTerminal] =
        destinationTerminalId;
    p.properties.remove(PK::Terminal::Destinations);
    return doc->updateTerminal(originTerminalId, p);
}

bool ScenarioEditService::setOriginDestinationRoutes(
    Scenario::ScenarioDocument              *doc,
    const QString                           &originTerminalId,
    const QList<Scenario::DestinationRoute> &routes)
{
    using namespace Scenario;
    if (!validateDestinationRoutes(doc, originTerminalId, routes))
    {
        qCWarning(lcGuiScene)
            << "ScenarioEditService::setOriginDestinationRoutes:"
            << "invalid routes for origin" << originTerminalId
            << "routeCount=" << routes.size();
        return false;
    }

    TerminalPlacement p = doc->terminals[originTerminalId];
    p.properties[PK::Terminal::Destinations] =
        destinationRoutesToVariantList(routes);
    p.properties.remove(PK::Terminal::DestinationTerminal);
    return doc->updateTerminal(originTerminalId, p);
}

bool ScenarioEditService::clearOriginDestinations(
    Scenario::ScenarioDocument *doc,
    const QString              &originTerminalId)
{
    using namespace Scenario;
    if (!destinationExists(doc, originTerminalId))
        return false;

    TerminalPlacement p = doc->terminals[originTerminalId];
    p.properties.remove(PK::Terminal::DestinationTerminal);
    p.properties.remove(PK::Terminal::Destinations);
    return doc->updateTerminal(originTerminalId, p);
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

bool ScenarioEditService::updateComparisonSnapshots(
    Scenario::ScenarioDocument *doc,
    const QList<QJsonObject>   &snapshots)
{
    if (!doc)
        return false;
    qCInfo(lcGuiScene)
        << "ScenarioEditService::updateComparisonSnapshots"
        << "snapshots=" << snapshots.size();
    doc->comparisonSnapshots = snapshots;
    return true;
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
