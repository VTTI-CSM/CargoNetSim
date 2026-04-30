#include "ScenarioDocument.h"

#include "Backend/Commons/LogCategories.h"
#include "PropertyKeys.h"

#include <containerLib/container.h>  // ContainerCore::Container full definition

#include <QtMath>   // qQNaN()

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

ScenarioDocument::ScenarioDocument(QObject *parent)
    : QObject(parent)
{
}

namespace
{
// Free every Container* owned by the per-terminal pools and empty the map.
// Centralises the ownership contract documented on `m_containersByTerminal`
// so the destructor, `reset`, and per-terminal replacement share one
// implementation.
void deleteAllContainers(
    QMap<QString, QList<ContainerCore::Container *>> &byTerminal)
{
    for (auto &list : byTerminal)
        qDeleteAll(list);
    byTerminal.clear();
}

void deleteContainersForTerminal(
    QMap<QString, QList<ContainerCore::Container *>> &byTerminal,
    const QString                                    &terminalId)
{
    auto it = byTerminal.find(terminalId);
    if (it == byTerminal.end())
        return;
    qDeleteAll(*it);
    byTerminal.erase(it);
}

bool endpointReferencesTerminal(const QString &endpoint,
                                const QString &terminalId,
                                const QString &region)
{
    if (endpoint == terminalId)
        return true;
    if (region.isEmpty())
        return false;
    return endpoint == region + QLatin1Char('/') + terminalId;
}

bool terminalBelongsToRegion(const ScenarioDocument &doc,
                             const QString          &terminalId,
                             const QString          &region)
{
    const auto it = doc.terminals.constFind(terminalId);
    return it != doc.terminals.constEnd() && it->region == region;
}

bool terminalHasRegionScopedDependencies(const ScenarioDocument &doc,
                                         const QString          &terminalId,
                                         const QString          &region)
{
    for (const NodeLinkage &linkage : doc.linkages)
    {
        if (linkage.terminalId == terminalId)
            return true;
    }

    for (const Connection &connection : doc.connections)
    {
        if (connection.fromTerminalId == terminalId
            || connection.toTerminalId == terminalId)
        {
            return true;
        }
    }

    for (const GlobalLink &link : doc.globalLinks)
    {
        if (endpointReferencesTerminal(link.fromTerminalId,
                                       terminalId, region)
            || endpointReferencesTerminal(link.toTerminalId,
                                          terminalId, region))
        {
            return true;
        }
    }

    return false;
}

bool connectionIdentityMatches(
    const Connection &connection,
    const QString &fromId,
    const QString &toId,
    TransportationTypes::TransportationMode mode)
{
    return connection.fromTerminalId == fromId
        && connection.toTerminalId == toId
        && connection.mode == mode;
}

bool globalLinkIdentityMatches(
    const GlobalLink &link,
    const QString &fromId,
    const QString &toId,
    TransportationTypes::TransportationMode mode)
{
    return link.fromTerminalId == fromId
        && link.toTerminalId == toId
        && link.mode == mode;
}

bool linkageIdentityMatches(const NodeLinkage &linkage,
                            const QString &terminalId,
                            const QString &networkName,
                            int nodeId)
{
    return linkage.terminalId == terminalId
        && linkage.networkName == networkName
        && linkage.nodeId == nodeId;
}
} // namespace

ScenarioDocument::~ScenarioDocument()
{
    deleteAllContainers(m_containersByTerminal);
}

void ScenarioDocument::reset()
{
    qCInfo(lcScenario) << "ScenarioDocument::reset: clearing all data";
    simulation  = SimulationSettings{};
    output      = OutputSpec{};
    fleet       = FleetSpec{};
    regions.clear();
    terminals.clear();
    linkages.clear();
    connections.clear();
    globalLinks.clear();
    comparisonSnapshots.clear();
    globalLinkStrategy = LinkageStrategy::Manual;
    globalLinkAutoRules.clear();
    globalLinkAutoRuleParams.clear();
    deleteAllContainers(m_containersByTerminal);
    emit documentReset();
}

// ---- Read-only query accessors ----

QList<NodeLinkage>
ScenarioDocument::linkagesFor(const QString     &terminalId,
                              NetworkSpec::Type  type) const
{
    qCDebug(lcScenario) << "ScenarioDocument::linkagesFor: terminalId:" << terminalId
                        << "type:" << static_cast<int>(type);
    QList<NodeLinkage> out;
    const auto terminalIt = terminals.constFind(terminalId);
    if (terminalIt == terminals.constEnd())
    {
        qCDebug(lcScenario) << "ScenarioDocument::linkagesFor:"
                            << "terminal not found";
        return out;
    }
    const auto regionIt = regions.constFind(terminalIt->region);
    if (regionIt == regions.constEnd())
    {
        qCDebug(lcScenario) << "ScenarioDocument::linkagesFor:"
                            << "terminal region not found";
        return out;
    }

    for (const NodeLinkage &l : linkages)
    {
        if (l.terminalId != terminalId) continue;

        // Resolve networkName within the terminal's owning region only.
        // Network names can repeat across regions; using any matching region
        // would bind a terminal to another region's network by accident.
        auto networkIt = regionIt->networks.constFind(l.networkName);
        if (networkIt != regionIt->networks.constEnd()
            && networkIt->type == type)
        {
            out.append(l);
        }
    }
    qCDebug(lcScenario) << "ScenarioDocument::linkagesFor: found" << out.size() << "linkages";
    return out;
}

QPointF ScenarioDocument::globalPositionOf(const QString &terminalId) const
{
    qCDebug(lcScenario) << "ScenarioDocument::globalPositionOf: terminalId:" << terminalId;
    const auto it = terminals.constFind(terminalId);
    if (it == terminals.constEnd())
    {
        qCWarning(lcScenario) << "ScenarioDocument::globalPositionOf:"
                              << "terminal not found:" << terminalId;
        return QPointF(qQNaN(), qQNaN());
    }

    const TerminalPlacement &t = *it;
    const auto rit = regions.constFind(t.region);
    if (rit == regions.constEnd()) return QPointF(qQNaN(), qQNaN());

    // Only LatLon-mode placements have directly usable lat/lon. Scene and
    // NetworkNode modes require an external coordinate resolver.
    if (t.mode != TerminalPlacement::PositionMode::LatLon)
        return QPointF(qQNaN(), qQNaN());

    const RegionSpec &r = *rit;
    const double lat = r.globalPosition.latitude
                     + (t.latLon.latitude  - r.localOrigin.latitude);
    const double lon = r.globalPosition.longitude
                     + (t.latLon.longitude - r.localOrigin.longitude);
    return QPointF(lon, lat);  // Qt convention: x=lon, y=lat
}

QStringList ScenarioDocument::originTerminalIds() const
{
    QStringList result;
    for (auto it = terminals.constBegin();
         it != terminals.constEnd(); ++it)
    {
        if (isOrigin(it.key())) result << it.key();
    }
    qCDebug(lcScenario) << "ScenarioDocument::originTerminalIds: found" << result.size() << "origins";
    return result;
}

bool ScenarioDocument::hasAnyOrigin() const
{
    for (auto it = terminals.constBegin();
         it != terminals.constEnd(); ++it)
    {
        if (isOrigin(it.key())) return true;
    }
    return false;
}

const QList<ContainerCore::Container *> &
ScenarioDocument::containersAt(const QString &terminalId) const
{
    static const QList<ContainerCore::Container *> s_empty;
    const auto it = m_containersByTerminal.constFind(terminalId);
    return (it != m_containersByTerminal.constEnd()) ? *it : s_empty;
}

QList<DestinationRoute>
ScenarioDocument::destinationsFor(
    const QString &originTerminalId) const
{
    const auto tit = terminals.constFind(originTerminalId);
    if (tit == terminals.constEnd()) return {};
    if (!isOrigin(originTerminalId)) return {};

    const QMap<QString, QVariant> &props = tit->properties;

    // Preferred form: explicit routing list with per-entry fractions.
    // QVariantList → DestinationRoute{terminal, fraction}.
    if (props.contains(PK::Terminal::Destinations))
    {
        QList<DestinationRoute> routes;
        const auto raw =
            props.value(PK::Terminal::Destinations).toList();
        routes.reserve(raw.size());
        for (const auto &entry : raw)
        {
            const auto m = entry.toMap();
            DestinationRoute r;
            r.terminal = m.value(PK::Terminal::DestTerminal).toString();
            r.fraction =
                m.value(PK::Terminal::DestFraction, 1.0).toDouble();
            routes.append(r);
        }
        return routes;
    }

    // Shorthand: scalar `destination_terminal: D` → [{D, 1.0}].
    if (props.contains(PK::Terminal::DestinationTerminal))
    {
        return { DestinationRoute{
            props.value(PK::Terminal::DestinationTerminal).toString(),
            1.0} };
    }

    return {};
}

bool ScenarioDocument::isOrigin(const QString &terminalId) const
{
    if (!containersAt(terminalId).isEmpty()) return true;
    const auto it = terminals.constFind(terminalId);
    if (it == terminals.constEnd()) return false;
    return it->properties
              .value(PK::Terminal::InitialContainerCount, 0)
              .toInt() > 0;
}

bool ScenarioDocument::isDestination(const QString &terminalId) const
{
    if (terminalId.isEmpty()) return false;
    for (auto it = terminals.constBegin();
         it != terminals.constEnd(); ++it)
    {
        // An origin pointing at itself is a validator error, not a
        // destination relationship — skip the self-edge defensively.
        if (it.key() == terminalId) continue;
        if (!isOrigin(it.key()))    continue;
        for (const DestinationRoute &r : destinationsFor(it.key()))
        {
            if (r.terminal == terminalId) return true;
        }
    }
    return false;
}

int ScenarioDocument::originContainerCount(
    const QString &terminalId) const
{
    const auto it = terminals.constFind(terminalId);
    const int  specCount = (it != terminals.constEnd())
        ? it->properties
              .value(PK::Terminal::InitialContainerCount, 0)
              .toInt()
        : 0;
    return qMax(specCount, containersAt(terminalId).size());
}

QList<ContainerCore::Container *>
ScenarioDocument::originContainers() const
{
    QList<ContainerCore::Container *> all;
    for (const auto &list : m_containersByTerminal)
        all.append(list);
    return all;
}

void ScenarioDocument::setOriginContainers(
    const QString                     &terminalId,
    QList<ContainerCore::Container *>  containers)
{
    qCDebug(lcScenario) << "ScenarioDocument::setOriginContainers: terminalId:" << terminalId
                        << "count:" << containers.size();
    if (terminalId.isEmpty() || !terminals.contains(terminalId))
    {
        qCWarning(lcScenario)
            << "ScenarioDocument::setOriginContainers:"
            << "unknown terminal id:" << terminalId
            << "discarding containers=" << containers.size();
        qDeleteAll(containers);
        return;
    }

    // Replace semantics: free any prior pool at this id, then install the
    // new per-origin pool so multiple origins coexist without interference.
    auto it = m_containersByTerminal.find(terminalId);
    if (it != m_containersByTerminal.end())
    {
        qDeleteAll(*it);
        *it = std::move(containers);
    }
    else
    {
        m_containersByTerminal.insert(terminalId, std::move(containers));
    }
}

bool ScenarioDocument::addRegion(const RegionSpec &r)
{
    qCDebug(lcScenario) << "ScenarioDocument::addRegion:" << r.name;
    if (r.name.isEmpty())         return false;
    if (regions.contains(r.name)) return false;
    regions.insert(r.name, r);
    emit regionAdded(r.name);
    return true;
}

bool ScenarioDocument::removeRegion(const QString &name)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeRegion:" << name;
    if (!regions.contains(name)) return false;

    // Cascade: drop every terminal owned by this region. removeTerminal in
    // turn cascades into linkages / connections / global_links, so we don't
    // have to walk those lists here.
    const QStringList ids = terminals.keys();  // copy — we mutate below.
    for (const QString &id : ids)
    {
        if (terminals.value(id).region == name)
            removeTerminal(id);
    }

    regions.remove(name);
    emit regionRemoved(name);
    return true;
}

bool ScenarioDocument::updateRegion(const QString &name, const RegionSpec &spec)
{
    qCDebug(lcScenario) << "ScenarioDocument::updateRegion:" << name;
    if (name.isEmpty())              return false;
    if (!regions.contains(name))     return false;
    if (spec.name != name)           return false;  // renames go via renameRegion

    regions[name] = spec;
    emit regionChanged(name);
    return true;
}

bool ScenarioDocument::renameRegion(const QString &oldName, const QString &newName)
{
    qCDebug(lcScenario) << "ScenarioDocument::renameRegion:" << oldName << "->" << newName;
    if (oldName == newName)             return true;
    if (newName.isEmpty())              return false;
    if (!regions.contains(oldName))     return false;
    if ( regions.contains(newName))     return false;

    RegionSpec r = regions.take(oldName);
    r.name = newName;
    regions.insert(newName, r);

    // Re-anchor every terminal that points at oldName.
    int movedCount = 0;
    for (auto it = terminals.begin(); it != terminals.end(); ++it)
    {
        if (it.value().region == oldName)
        {
            it.value().region = newName;
            ++movedCount;
            emit terminalChanged(it.key());
        }
    }
    qCDebug(lcScenario) << "ScenarioDocument::renameRegion:"
                        << movedCount << "terminals moved from"
                        << oldName << "to" << newName;

    // Re-anchor connection.region (redundant copy of endpoint regions).
    for (Connection &c : connections)
    {
        if (c.region == oldName) c.region = newName;
    }

    // Re-anchor qualified "<oldName>/<id>" prefixes in global_link endpoints.
    const QString oldPrefix = oldName + QLatin1Char('/');
    const QString newPrefix = newName + QLatin1Char('/');
    for (GlobalLink &g : globalLinks)
    {
        if (g.fromTerminalId.startsWith(oldPrefix))
            g.fromTerminalId = newPrefix
                + g.fromTerminalId.mid(oldPrefix.size());
        if (g.toTerminalId.startsWith(oldPrefix))
            g.toTerminalId   = newPrefix
                + g.toTerminalId.mid(oldPrefix.size());
    }

    emit regionRenamed(oldName, newName);
    return true;
}

bool ScenarioDocument::addTerminal(const TerminalPlacement &t)
{
    qCDebug(lcScenario) << "ScenarioDocument::addTerminal:" << t.id
                        << "region:" << t.region;
    if (t.id.isEmpty())                 return false;
    if (terminals.contains(t.id))       return false;
    if (!regions.contains(t.region))    return false;   // region must exist
    terminals.insert(t.id, t);
    emit terminalAdded(t.id);
    return true;
}

bool ScenarioDocument::removeTerminal(const QString &id)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeTerminal:" << id;
    if (!terminals.contains(id)) return false;

    // Snapshot region before we erase the terminal — needed to match the
    // qualified "<region>/<id>" form used inside global_link endpoint IDs.
    const QString region    = terminals.value(id).region;
    const QString qualified = region.isEmpty()
        ? QString()
        : region + QLatin1Char('/') + id;

    // Cascade: linkages referencing this terminal.
    int removedLinkages = 0;
    for (int i = linkages.size() - 1; i >= 0; --i)
    {
        if (linkages.at(i).terminalId == id)
        {
            const QString nw  = linkages.at(i).networkName;
            const int     nid = linkages.at(i).nodeId;
            linkages.removeAt(i);
            ++removedLinkages;
            emit linkageRemoved(id, nw, nid);
        }
    }

    // Cascade: connections referencing this terminal (either endpoint).
    int removedConnections = 0;
    for (int i = connections.size() - 1; i >= 0; --i)
    {
        const Connection &c = connections.at(i);
        if (c.fromTerminalId == id || c.toTerminalId == id)
        {
            const QString from = c.fromTerminalId;
            const QString to   = c.toTerminalId;
            const TransportationTypes::TransportationMode mode =
                c.mode;
            connections.removeAt(i);
            ++removedConnections;
            emit connectionRemoved(from, to, mode);
        }
    }

    // Cascade: global_links referencing this terminal, bare OR qualified.
    int removedGlobalLinks = 0;
    for (int i = globalLinks.size() - 1; i >= 0; --i)
    {
        const GlobalLink &g = globalLinks.at(i);
        const bool hit =
               g.fromTerminalId == id || g.toTerminalId == id
            || (!qualified.isEmpty() &&
                (g.fromTerminalId == qualified
                 || g.toTerminalId   == qualified));
        if (hit)
        {
            const QString from = g.fromTerminalId;
            const QString to   = g.toTerminalId;
            const TransportationTypes::TransportationMode mode =
                g.mode;
            globalLinks.removeAt(i);
            ++removedGlobalLinks;
            emit globalLinkRemoved(from, to, mode);
        }
    }

    qCDebug(lcScenario) << "ScenarioDocument::removeTerminal:"
                        << "cascaded removals for" << id
                        << "- linkages=" << removedLinkages
                        << "connections=" << removedConnections
                        << "globalLinks=" << removedGlobalLinks;

    deleteContainersForTerminal(m_containersByTerminal, id);
    terminals.remove(id);
    emit terminalRemoved(id);
    return true;
}

bool ScenarioDocument::updateTerminal(const QString &id, const TerminalPlacement &t)
{
    qCDebug(lcScenario) << "ScenarioDocument::updateTerminal:" << id;
    if (!terminals.contains(id))     return false;
    if (t.id != id)                  return false;  // id must stay pinned to the key
    if (!regions.contains(t.region)) return false;

    const TerminalPlacement current = terminals.value(id);
    if (current.region != t.region
        && terminalHasRegionScopedDependencies(*this, id,
                                               current.region))
    {
        qCWarning(lcScenario)
            << "ScenarioDocument::updateTerminal: region change rejected for"
            << id << "from" << current.region << "to" << t.region
            << "because the terminal has linkages or routes. Remove or"
               "recreate those dependencies explicitly before moving it.";
        return false;
    }

    terminals.insert(id, t);
    emit terminalChanged(id);
    return true;
}

bool ScenarioDocument::addNetwork(const QString &region, const NetworkSpec &n)
{
    qCDebug(lcScenario) << "ScenarioDocument::addNetwork: region:" << region
                        << "network:" << n.name;
    if (n.name.isEmpty())                return false;
    if (!regions.contains(region))       return false;
    auto &r = regions[region];
    if (r.networks.contains(n.name))     return false;
    r.networks.insert(n.name, n);
    emit networkAdded(region, n.name);
    return true;
}

bool ScenarioDocument::removeNetwork(const QString &region, const QString &network)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeNetwork: region:" << region
                        << "network:" << network;
    if (!regions.contains(region))        return false;
    auto &r = regions[region];
    if (!r.networks.contains(network))    return false;

    int removedLinkages = 0;
    for (int i = linkages.size() - 1; i >= 0; --i)
    {
        const NodeLinkage &linkage = linkages.at(i);
        if (linkage.networkName == network
            && terminalBelongsToRegion(*this, linkage.terminalId, region))
        {
            const QString terminalId = linkage.terminalId;
            const int nodeId = linkage.nodeId;
            linkages.removeAt(i);
            ++removedLinkages;
            emit linkageRemoved(terminalId, network, nodeId);
        }
    }

    r.networks.remove(network);
    qCDebug(lcScenario)
        << "ScenarioDocument::removeNetwork: cascaded"
        << removedLinkages << "linkage(s)";
    emit networkRemoved(region, network);
    return true;
}

bool ScenarioDocument::renameNetwork(const QString &region,
                                     const QString &oldName,
                                     const QString &newName)
{
    qCDebug(lcScenario) << "ScenarioDocument::renameNetwork:"
                        << region << oldName << "->" << newName;
    if (oldName == newName)              return true;
    if (newName.isEmpty())               return false;
    if (!regions.contains(region))       return false;
    auto &r = regions[region];
    if (!r.networks.contains(oldName))       return false;
    if (r.networks.contains(newName))        return false;
    NetworkSpec spec = r.networks.take(oldName);
    spec.name        = newName;
    r.networks.insert(newName, spec);

    int reanchoredLinkages = 0;
    for (NodeLinkage &linkage : linkages)
    {
        if (linkage.networkName == oldName
            && terminalBelongsToRegion(*this, linkage.terminalId, region))
        {
            const QString terminalId = linkage.terminalId;
            const int nodeId = linkage.nodeId;
            linkage.networkName = newName;
            ++reanchoredLinkages;
            emit linkageChanged(terminalId, newName, nodeId);
        }
    }

    qCDebug(lcScenario)
        << "ScenarioDocument::renameNetwork: reanchored"
        << reanchoredLinkages << "linkage(s)";
    emit networkRenamed(region, oldName, newName);
    return true;
}

bool ScenarioDocument::addLinkage(const NodeLinkage &l)
{
    qCDebug(lcScenario) << "ScenarioDocument::addLinkage: terminal:" << l.terminalId
                        << "network:" << l.networkName << "node:" << l.nodeId;
    if (l.terminalId.isEmpty())                 return false;
    if (!terminals.contains(l.terminalId))      return false;
    for (const NodeLinkage &existing : linkages)
    {
        if (linkageIdentityMatches(existing, l.terminalId,
                                   l.networkName, l.nodeId))
        {
            qCWarning(lcScenario)
                << "ScenarioDocument::addLinkage: duplicate linkage rejected"
                << l.terminalId << l.networkName << "node=" << l.nodeId;
            return false;
        }
    }
    linkages.append(l);
    emit linkageAdded(l);
    return true;
}

bool ScenarioDocument::removeLinkage(const QString &terminalId,
                                     const QString &networkName, int nodeId)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeLinkage: terminal:" << terminalId
                        << "network:" << networkName << "node:" << nodeId;
    for (int i = 0; i < linkages.size(); ++i)
    {
        const NodeLinkage &l = linkages.at(i);
        if (l.terminalId == terminalId &&
            l.networkName == networkName &&
            l.nodeId      == nodeId)
        {
            linkages.removeAt(i);
            emit linkageRemoved(terminalId, networkName, nodeId);
            return true;
        }
    }
    return false;
}

bool ScenarioDocument::addConnection(const Connection &c)
{
    qCDebug(lcScenario) << "ScenarioDocument::addConnection: from:" << c.fromTerminalId
                        << "to:" << c.toTerminalId;
    if (c.fromTerminalId.isEmpty() || c.toTerminalId.isEmpty()) return false;
    if (!terminals.contains(c.fromTerminalId))                  return false;
    if (!terminals.contains(c.toTerminalId))                    return false;
    if (findConnectionIndex(c.fromTerminalId, c.toTerminalId, c.mode) >= 0)
    {
        qCWarning(lcScenario)
            << "ScenarioDocument::addConnection: duplicate route rejected"
            << c.fromTerminalId << "->" << c.toTerminalId
            << "mode=" << static_cast<int>(c.mode);
        return false;
    }
    connections.append(c);
    emit connectionAdded(c);
    return true;
}

bool ScenarioDocument::removeConnection(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeConnection: from:" << fromId
                        << "to:" << toId;
    const int i = findConnectionIndex(fromId, toId, mode);
    if (i >= 0)
    {
        connections.removeAt(i);
        emit connectionRemoved(fromId, toId, mode);
        return true;
    }
    return false;
}

int ScenarioDocument::findConnectionIndex(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode) const
{
    for (int i = 0; i < connections.size(); ++i)
    {
        const Connection &c = connections.at(i);
        if (c.fromTerminalId == fromId
            && c.toTerminalId == toId
            && c.mode == mode)
            return i;
    }
    return -1;
}

Connection *ScenarioDocument::findConnection(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode)
{
    const int i = findConnectionIndex(fromId, toId, mode);
    return i >= 0 ? &connections[i] : nullptr;
}

const Connection *ScenarioDocument::findConnection(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode) const
{
    const int i = findConnectionIndex(fromId, toId, mode);
    return i >= 0 ? &connections.at(i) : nullptr;
}

bool ScenarioDocument::addGlobalLink(const GlobalLink &g)
{
    qCDebug(lcScenario) << "ScenarioDocument::addGlobalLink: from:" << g.fromTerminalId
                        << "to:" << g.toTerminalId;
    if (g.fromTerminalId.isEmpty() || g.toTerminalId.isEmpty()) return false;
    if (findGlobalLinkIndex(g.fromTerminalId, g.toTerminalId, g.mode) >= 0)
    {
        qCWarning(lcScenario)
            << "ScenarioDocument::addGlobalLink: duplicate route rejected"
            << g.fromTerminalId << "->" << g.toTerminalId
            << "mode=" << static_cast<int>(g.mode);
        return false;
    }
    globalLinks.append(g);
    emit globalLinkAdded(g);
    return true;
}

bool ScenarioDocument::removeGlobalLink(
    const QString &fromQual, const QString &toQual,
    TransportationTypes::TransportationMode mode)
{
    qCDebug(lcScenario) << "ScenarioDocument::removeGlobalLink: from:" << fromQual
                        << "to:" << toQual;
    const int i = findGlobalLinkIndex(fromQual, toQual, mode);
    if (i >= 0)
    {
        globalLinks.removeAt(i);
        emit globalLinkRemoved(fromQual, toQual, mode);
        return true;
    }
    return false;
}

int ScenarioDocument::findGlobalLinkIndex(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode) const
{
    for (int i = 0; i < globalLinks.size(); ++i)
    {
        const GlobalLink &g = globalLinks.at(i);
        if (g.fromTerminalId == fromId
            && g.toTerminalId == toId
            && g.mode == mode)
            return i;
    }
    return -1;
}

GlobalLink *ScenarioDocument::findGlobalLink(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode)
{
    const int i = findGlobalLinkIndex(fromId, toId, mode);
    return i >= 0 ? &globalLinks[i] : nullptr;
}

const GlobalLink *ScenarioDocument::findGlobalLink(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode) const
{
    const int i = findGlobalLinkIndex(fromId, toId, mode);
    return i >= 0 ? &globalLinks.at(i) : nullptr;
}

bool ScenarioDocument::updateConnection(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode,
    const Connection &updated)
{
    qCDebug(lcScenario)
        << "ScenarioDocument::updateConnection:"
        << fromId << "->" << toId;
    const int i = findConnectionIndex(fromId, toId, mode);
    if (i >= 0)
    {
        if (!connectionIdentityMatches(updated, fromId, toId, mode))
        {
            qCWarning(lcScenario)
                << "ScenarioDocument::updateConnection:"
                << "route identity changes are not allowed; use"
                << "removeConnection/addConnection";
            return false;
        }
        connections[i] = updated;
        emit connectionChanged(fromId, toId, mode);
        return true;
    }
    qCWarning(lcScenario)
        << "ScenarioDocument::updateConnection: not found";
    return false;
}

bool ScenarioDocument::updateGlobalLink(
    const QString &fromId, const QString &toId,
    TransportationTypes::TransportationMode mode,
    const GlobalLink &updated)
{
    qCDebug(lcScenario)
        << "ScenarioDocument::updateGlobalLink:"
        << fromId << "->" << toId;
    const int i = findGlobalLinkIndex(fromId, toId, mode);
    if (i >= 0)
    {
        if (!globalLinkIdentityMatches(updated, fromId, toId, mode))
        {
            qCWarning(lcScenario)
                << "ScenarioDocument::updateGlobalLink:"
                << "route identity changes are not allowed; use"
                << "removeGlobalLink/addGlobalLink";
            return false;
        }
        globalLinks[i] = updated;
        emit globalLinkChanged(fromId, toId, mode);
        return true;
    }
    qCWarning(lcScenario)
        << "ScenarioDocument::updateGlobalLink: not found";
    return false;
}

bool ScenarioDocument::updateLinkage(
    const QString &terminalId, const QString &networkName,
    int nodeId, const NodeLinkage &updated)
{
    qCDebug(lcScenario)
        << "ScenarioDocument::updateLinkage:"
        << terminalId << networkName << nodeId;
    for (int i = 0; i < linkages.size(); ++i)
    {
        const NodeLinkage &l = linkages.at(i);
        if (l.terminalId  == terminalId
            && l.networkName == networkName
            && l.nodeId      == nodeId)
        {
            if (!linkageIdentityMatches(updated, terminalId,
                                        networkName, nodeId))
            {
                qCWarning(lcScenario)
                    << "ScenarioDocument::updateLinkage:"
                    << "linkage identity changes are not allowed; use"
                    << "removeLinkage/addLinkage";
                return false;
            }
            linkages[i] = updated;
            emit linkageChanged(terminalId, networkName, nodeId);
            return true;
        }
    }
    qCWarning(lcScenario)
        << "ScenarioDocument::updateLinkage: not found";
    return false;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
