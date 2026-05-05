#pragma once

#include "Connection.h"
#include "FleetSpec.h"
#include "GlobalLink.h"
#include "NetworkSpec.h"  // for NetworkSpec::Type used by linkagesFor()
#include "NodeLinkage.h"
#include "OutputSpec.h"
#include "RegionSpec.h"
#include "SimulationSettings.h"
#include "TerminalPlacement.h"

#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QObject>
#include <QPointF>        // for globalPositionOf() return type
#include <QString>

// ContainerCore is already a CargoNetSimBackend dependency (used in clients,
// items). Forward-declare here and include in the .cpp to keep the header
// lightweight.
namespace ContainerCore { class Container; }

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// One routing entry for an origin terminal — a single destination id
/// plus the fraction of the origin's container pool that ends up there.
///
/// Authored via the YAML `destinations: [{terminal, fraction}]` list.
/// Scalar `destination_terminal: D` is normalized to a single-entry list
/// `{D, 1.0}` at read time, so downstream code (applier, CLI, executor)
/// never branches on "was it written as scalar or list" — it iterates.
struct DestinationRoute
{
    QString terminal;
    double  fraction = 1.0;
};

/// The single owner of all scenario state.
///
/// Invariants (enforced by mutator methods; do NOT bypass via direct QMap access):
///   * regions[k].name == k for every key k
///   * terminals[k].id   == k for every key k
///   * Every terminal.region must be an existing regions[].name
///   * Every linkage.terminalId must be an existing terminals[].id
///   * Every connection.{fromTerminalId,toTerminalId} must exist in terminals
///   * connection.region == terminals[connection.from].region
///                       == terminals[connection.to].region
///
/// Cascading semantics (ALL mutators preserve the invariants listed above):
///   removeTerminal(id)        → also removes every linkage / connection /
///                               global_link that references id (emits the
///                               corresponding *Removed signal for each).
///   removeRegion(name)        → for every terminal t with t.region==name
///                               it calls removeTerminal(t.id) first (so
///                               linkages/connections/global_links cascade
///                               automatically), THEN drops the region.
///   renameRegion(old, new)    → re-anchors every referencing terminal.region,
///                               connection.region, and qualified "old/id"
///                               prefix inside global_link endpoint IDs;
///                               emits terminalChanged for each moved
///                               terminal plus regionRenamed exactly once.
///
/// Direct mutation of member QMaps outside of the provided methods is a
/// programmer error (but not enforced at compile time since the maps are
/// public for read convenience).
class ScenarioDocument : public QObject
{
    Q_OBJECT

public:
    explicit ScenarioDocument(QObject *parent = nullptr);
    ~ScenarioDocument() override;

    // Top-level state (public for read access; mutate only through methods).
    SimulationSettings simulation;
    OutputSpec         output;
    FleetSpec          fleet;
    QMap<QString, RegionSpec>        regions;
    QMap<QString, TerminalPlacement> terminals;
    QList<NodeLinkage>               linkages;
    QList<Connection>                connections;
    QList<GlobalLink>                globalLinks;
    QList<QJsonObject>               comparisonSnapshots;

    // Scenario-scope auto-rules for cross-region links
    // (e.g. "sea_port_pairs_within_km: 10000"). Global links have no
    // region — these live at the document level. Defaults are safe: the
    // resolver treats Manual+empty rules as "return manual entries verbatim".
    LinkageStrategy         globalLinkStrategy = LinkageStrategy::Manual;
    QList<QString>          globalLinkAutoRules;
    QMap<QString, QVariant> globalLinkAutoRuleParams;  // rule name → param

    /// Clear all state and emit documentReset().
    void reset();

    // Region CRUD. Each returns false and is a no-op on invariant violation.
    bool addRegion(const RegionSpec &r);
    bool removeRegion(const QString &name);
    bool renameRegion(const QString &oldName, const QString &newName);

    /// In-place update of an existing region's fields. The region's `name`
    /// MUST equal `name` (renames go through renameRegion which cascades).
    /// Returns false if the region does not exist or if the passed spec's
    /// name disagrees. Emits `regionChanged(name)` exactly once on success.
    /// Canonical update path for ScenarioEditService region edits; callers
    /// should not mutate `regions[name]` directly.
    bool updateRegion(const QString &name, const RegionSpec &spec);

    // Terminal CRUD.
    bool addTerminal(const TerminalPlacement &t);
    bool removeTerminal(const QString &id);
    /// Updates terminal fields without changing the terminal id. Region
    /// changes are accepted only when the terminal has no region-scoped
    /// linkages, connections, or global links; otherwise callers must edit
    /// those dependencies explicitly before moving the terminal.
    bool updateTerminal(const QString &id, const TerminalPlacement &t);

    // Network CRUD (nested inside a region). Removing a network drops its
    // terminal-node linkages in that region; renaming a network reanchors
    // those linkages to the new network name.
    bool addNetwork(const QString &region, const NetworkSpec &n);
    bool removeNetwork(const QString &region, const QString &network);
    bool renameNetwork(const QString &region,
                       const QString &oldName,
                       const QString &newName);

    // Linkage / Connection / GlobalLink CRUD.
    bool addLinkage(const NodeLinkage &l);
    bool removeLinkage(const QString &terminalId,
                       const QString &networkName, int nodeId);

    bool addConnection(const Connection &c);
    bool removeConnection(const QString &fromId, const QString &toId,
                          TransportationTypes::TransportationMode mode);
    int findConnectionIndex(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode) const;
    Connection *findConnection(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode);
    const Connection *findConnection(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode) const;

    bool addGlobalLink(const GlobalLink &g);
    bool removeGlobalLink(const QString &fromQual, const QString &toQual,
                          TransportationTypes::TransportationMode mode);
    int findGlobalLinkIndex(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode) const;
    GlobalLink *findGlobalLink(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode);
    const GlobalLink *findGlobalLink(
        const QString &fromId, const QString &toId,
        TransportationTypes::TransportationMode mode) const;

    // Update payload fields for an existing identity. The identity fields
    // themselves are immutable here; route/linkage identity changes must be
    // represented explicitly as remove + add so duplicate checks and lifecycle
    // signals stay correct.
    bool updateConnection(const QString &fromId,
                          const QString &toId,
                          TransportationTypes::TransportationMode mode,
                          const Connection &updated);
    bool updateGlobalLink(const QString &fromId,
                          const QString &toId,
                          TransportationTypes::TransportationMode mode,
                          const GlobalLink &updated);
    bool updateLinkage(const QString &terminalId,
                       const QString &networkName,
                       int nodeId,
                       const NodeLinkage &updated);

    // ---- Read-only query accessors ----

    /// Returns every linkage whose `terminalId` matches AND whose
    /// `networkName` resolves to a network of the requested type in the
    /// terminal's owning region. Linkages pointing at unknown networks are
    /// skipped.
    QList<NodeLinkage> linkagesFor(const QString     &terminalId,
                                   NetworkSpec::Type  type) const;

    /// Computes the terminal's global lat/lon per design spec §4:
    ///   global_lat = region.globalPosition.lat
    ///              + (placement.latLon.latitude  - region.localOrigin.latitude)
    ///   global_lon = region.globalPosition.lon
    ///              + (placement.latLon.longitude - region.localOrigin.longitude)
    /// Returns QPointF(lon, lat) to match Qt scene conventions. For
    /// placements in `PositionMode::Scene` or `NetworkNode`, this accessor
    /// returns an invalid QPointF (NaN) so callers can branch to an
    /// appropriate coordinate resolver. Terminal or region not found
    /// also returns an invalid QPointF.
    QPointF globalPositionOf(const QString &terminalId) const;

    /// Converts a global WGS84 point to the region-local lat/lon stored on
    /// terminal placements:
    ///   local_lat = global_lat - (region.globalPosition.lat
    ///                             - region.localOrigin.lat)
    ///   local_lon = global_lon - (region.globalPosition.lon
    ///                             - region.localOrigin.lon)
    /// Returns QPointF(lon, lat). Missing regions or non-finite inputs return
    /// an invalid QPointF (NaN, NaN); callers must not silently persist the
    /// global point as a local point.
    QPointF localPositionInRegion(const QString &regionName,
                                  const QPointF &globalLonLat) const;

    /// Terminal IDs for all origins. Derived from `isOrigin(id)` —
    /// works pre- and post-applier. Pre-applier: ids with
    /// `initial_container_count > 0`. Post-applier: identical set
    /// (the applier writes a pool only when the count was set).
    /// Single source of truth shared by CLI (PathDiscovery,
    /// ScenarioExecutor) and GUI (pre-simulation gates).
    QStringList originTerminalIds() const;

    /// Containers seeded at @p terminalId. Empty list if the id has no
    /// seeded pool. Returned by const-ref — safe for tight-loop reads
    /// since the underlying storage (`m_containersByTerminal`) is stable
    /// between `setOriginContainers` / `reset` calls.
    const QList<ContainerCore::Container *> &
    containersAt(const QString &terminalId) const;

    /// Destination routes for an origin terminal. Always returns a
    /// normalized list — scalar `destination_terminal: D` becomes
    /// `[{D, 1.0}]`; fractioned `destinations: [{terminal, fraction}]`
    /// parses directly. Empty list when the terminal is not an origin
    /// or no destination is authored.
    ///
    /// Computed on demand by parsing the terminal's `properties` map —
    /// no cached state, no invalidation concerns.
    QList<DestinationRoute>
    destinationsFor(const QString &originTerminalId) const;

    /// True iff the terminal is an origin: it has at least one seeded
    /// container in `m_containersByTerminal` (post-applier state) OR its
    /// `properties["initial_container_count"]` is > 0 (pre-applier / live
    /// GUI authoring state). Single source of truth for "is this an
    /// origin?" — CLI and GUI both call this. Never compare
    /// `terminals[id].type` against the literal string "Origin".
    bool isOrigin(const QString &terminalId) const;

    /// True iff @p terminalId appears as a destination of any origin in
    /// the document — either via the scalar `destination_terminal`
    /// property or via the `destinations:` list. Mirrors `isOrigin` as
    /// the CLI/GUI single source of truth; never compare
    /// `terminals[id].type` against the literal string "Destination".
    bool isDestination(const QString &terminalId) const;

    /// Convenience predicate: "is there at least one origin in the
    /// document?". Short-circuits on the first hit. Use in GUI
    /// pre-simulation gates instead of scanning the scene.
    bool hasAnyOrigin() const;

    /// Container pool size at @p terminalId — `max(initial_container_count
    /// property, containersAt(id).size())`. Single accessor that works in
    /// both phases: pre-applier (only the property is set) and post-applier
    /// (the typed pool is the authoritative size). Returns 0 for unknown
    /// terminals or non-origins.
    int originContainerCount(const QString &terminalId) const;

    /// Flattened view over every seeded container across all origins.
    /// Preserved for allocation and live execution planning paths that
    /// consume the per-origin pools without taking ownership.
    ///
    /// Returned by value because the list is composed on the fly from
    /// `m_containersByTerminal`.
    QList<ContainerCore::Container *> originContainers() const;

    /// Applier-side mutator (called by `ScenarioApplier::applyOriginContainers`).
    /// Replaces any existing pool at @p terminalId (freeing the prior
    /// owned pointers); ownership of the passed pointers transfers to
    /// the document. A subsequent `reset()` or destructor `qDeleteAll`s
    /// them.
    void setOriginContainers(const QString                     &terminalId,
                             QList<ContainerCore::Container *>  containers);

signals:
    void documentReset();
    void regionAdded(const QString &name);
    void regionRemoved(const QString &name);
    void regionRenamed(const QString &oldName, const QString &newName);
    void regionChanged(const QString &name);
    void networkAdded(const QString &region, const QString &network);
    void networkRemoved(const QString &region, const QString &network);
    void networkRenamed(const QString &region,
                        const QString &oldName,
                        const QString &newName);
    void terminalAdded(const QString &id);
    void terminalRemoved(const QString &id);
    void terminalChanged(const QString &id);
    void linkageAdded(const NodeLinkage &l);
    void linkageRemoved(const QString &terminalId,
                        const QString &networkName, int nodeId);
    void connectionAdded(const Connection &c);
    void connectionRemoved(const QString &fromId, const QString &toId,
                           TransportationTypes::TransportationMode mode);
    void globalLinkAdded(const GlobalLink &g);
    void globalLinkRemoved(const QString &fromQual,
                           const QString &toQual,
                           TransportationTypes::TransportationMode mode);
    void connectionChanged(const QString &fromId,
                           const QString &toId,
                           TransportationTypes::TransportationMode mode);
    void globalLinkChanged(const QString &fromId,
                           const QString &toId,
                           TransportationTypes::TransportationMode mode);
    void linkageChanged(const QString &terminalId,
                        const QString &networkName,
                        int nodeId);
    void simulationSettingsChanged();
    void fleetChanged();
    void originContainersChanged(const QString &terminalId);

private:
    /// Origin container pools keyed by origin terminal id. Owned: every
    /// `Container*` in every value list is freed in `reset()`, the
    /// destructor, and when `setOriginContainers(id, …)` replaces an
    /// entry. A terminal is an origin iff it has a non-empty list here;
    /// `originTerminalIds()` + `containersAt(id)` are the read API.
    QMap<QString, QList<ContainerCore::Container *>>
        m_containersByTerminal;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
