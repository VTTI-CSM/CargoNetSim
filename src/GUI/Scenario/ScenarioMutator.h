#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/FleetSpec.h"
#include "Backend/Scenario/SimulationSettings.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
class ScenarioDocument;
} // namespace Scenario
} // namespace Backend

namespace GUI {
namespace Scenario {

/**
 * @brief GUI-side mutator facade over ScenarioDocument.
 *
 * Every GUI action that changes the scenario — create terminal, link
 * terminal to a network node, create a connection, move a terminal,
 * update region coordinates — routes through one static method here.
 *
 * Purpose: ensure CLI (Plan 5, which mutates via direct ScenarioDocument
 * calls driven by YAML parsing) and GUI (this module, driven by user
 * interaction) reach the same document with the same invariants enforced
 * in one place.
 *
 * All methods are static and stateless; the document pointer is the only
 * state, passed in at each call. Coordinates are supplied as QPointF using
 * Qt's (x, y) = (longitude, latitude) convention.
 */
class ScenarioMutator
{
public:
    // ---------------- terminals ----------------

    /// Generates a unique id, populates default properties from
    /// TerminalTypeDefaults for @p terminalType, and inserts the placement
    /// at @p localLatLon via PositionMode::LatLon. Returns the generated id
    /// on success, or an empty QString on failure.
    static QString createTerminal(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalType,
        const QString                       &region,
        const QPointF                       &localLatLon,
        Backend::Scenario::TerminalPlacement::TerminalRole role =
            Backend::Scenario::TerminalPlacement::TerminalRole::Transit);

    static bool setTerminalRole(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        Backend::Scenario::TerminalPlacement::TerminalRole role);

    /// Change a terminal's type. Resets properties to the new type's defaults
    /// (preserving the current role and applying role side-effects), clears any
    /// per-terminal interface override so runtime derives from the new type, and
    /// emits terminalChanged so the scene item and PropertiesPanel refresh.
    static bool setTerminalType(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        const QString                       &newType);

    static bool removeTerminal(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId);

    static bool updateTerminalPosition(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        const QPointF                       &localLatLon);

    /// Set or clear a single property on a terminal's property bag.
    /// Sole authoring path for property-bag mutations from the GUI: the
    /// caller hands a key+value, the mutator reads the current placement,
    /// edits the bag, calls `ScenarioDocument::updateTerminal`, and the
    /// document emits `terminalChanged(id)` so any TerminalItem view
    /// can refresh its snapshot. Plan 8 introduces this so the new
    /// PropertiesPanel "Origin Configuration" group writes through one
    /// place instead of the legacy `TerminalItem::setProperty` direct-
    /// snapshot path. Returns false if @p doc is null or @p terminalId
    /// doesn't exist.
    ///
    /// To clear a key, pass an invalid QVariant (`QVariant{}`).
    static bool setTerminalProperty(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        const QString                       &key,
        const QVariant                      &value);

    // ---------------- linkages ----------------

    /// Link a terminal to a network node. @p networkName resolves to a
    /// specific rail-or-truck network at runtime via
    /// RegionData::findNetworkByName — the mutator does NOT take a
    /// `networkType` because NodeLinkage intentionally does not store it
    /// (see canonical-schema design: type is derived, not duplicated).
    static bool linkTerminalToNode(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        const QString                       &networkName,
        int                                  nodeId,
        Backend::Scenario::LinkageSource     source);

    static bool unlinkTerminalFromNode(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &terminalId,
        const QString                       &networkName,
        int                                  nodeId);

    // ---------------- connections ----------------

    /// Both endpoints must exist and belong to the same region; the
    /// mutator derives `Connection.region` from the from-terminal so the
    /// document-level invariant (from.region == to.region == c.region)
    /// holds without the caller having to pass it separately.
    static bool createConnection(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);
    static bool createConnection(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode,
        const QVariantMap                   &properties,
        Backend::Scenario::LinkageSource     source);

    static bool removeConnection(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);

    /// Cross-region GlobalLink creation. Unlike `createConnection`, the
    /// endpoints do NOT need to live in the same region — global links
    /// exist precisely to bridge regions. Both terminal ids must exist
    /// in the document's `terminals` map (defense-in-depth beyond the
    /// document's non-empty-id check).
    static bool createGlobalLink(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);
    static bool createGlobalLink(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode,
        const QVariantMap                   &properties,
        Backend::Scenario::LinkageSource     source);

    static bool removeGlobalLink(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromTerminalId,
        const QString                       &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);

    /// Set or update a single property on a connection or global link
    /// identified by (fromId, toId, mode). Searches regional connections
    /// first, then global links. Returns false if @p doc is null or no
    /// matching entry is found.
    static bool setConnectionProperty(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &fromId,
        const QString                       &toId,
        Backend::TransportationTypes::TransportationMode mode,
        const QString                       &key,
        const QVariant                      &value);

    // ---------------- regions ----------------

    /// Reads the current RegionSpec, edits its localOrigin, hands it back
    /// to ScenarioDocument::updateRegion — the mutator never writes to
    /// `regions[name]` directly; invariant enforcement + signal emission
    /// stay owned by the document.
    static bool updateRegionLocalOrigin(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &regionName,
        const QPointF                       &latLon);

    static bool updateRegionGlobalPosition(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &regionName,
        const QPointF                       &latLon);

    /// Register a new region in the document. Returns false if @p doc is
    /// null, @p spec.name is empty, or a region with that name already exists.
    /// Duplicate-name rejection is delegated to ScenarioDocument::addRegion.
    static bool addRegion(
        Backend::Scenario::ScenarioDocument     *doc,
        const Backend::Scenario::RegionSpec     &spec);

    /// Remove region @p name and cascade-delete all owned terminals,
    /// connections, and global links. Returns false if @p doc is null or
    /// the region is not found.
    static bool removeRegion(
        Backend::Scenario::ScenarioDocument     *doc,
        const QString                           &name);

    /// Update the color field of an existing region. @p colorHex should be
    /// QColor::name() format ("#RRGGBB"); the value is stored verbatim without
    /// validation. Returns false if @p doc is null or the region is not found.
    static bool updateRegionColor(
        Backend::Scenario::ScenarioDocument     *doc,
        const QString                           &name,
        const QString                           &colorHex);

    // ---------------- networks ----------------

    /// Record a new network in the document. @p spec must have name and type
    /// set; referencePoint should be populated from the region's localOrigin
    /// before calling. Returns false if @p doc is null or region not found.
    static bool addNetwork(
        Backend::Scenario::ScenarioDocument  *doc,
        const QString                        &region,
        const Backend::Scenario::NetworkSpec &spec);

    /// Remove a network from the document. Returns false if @p doc is null,
    /// region not found, or network not found.
    static bool removeNetwork(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &region,
        const QString                       &networkName);

    /// Rename a network in the document. Returns false if @p doc is null,
    /// region not found, oldName not found, or newName already exists.
    static bool renameNetwork(
        Backend::Scenario::ScenarioDocument *doc,
        const QString                       &region,
        const QString                       &oldName,
        const QString                       &newName);

    // ---------------- simulation ----------------

    /// Replace doc.simulation with @p settings. Caller must preserve
    /// YAML-only fields (endTime, dwellMethod, dwellParams) before calling.
    /// Returns false only when @p doc is null.
    static bool updateSimulationSettings(
        Backend::Scenario::ScenarioDocument         *doc,
        const Backend::Scenario::SimulationSettings &settings);

    /// Replaces doc->fleet with @p fleet. Caller must build the updated FleetSpec
    /// (merge existing + new paths) before calling.
    /// Returns false only when @p doc is null.
    static bool updateFleet(
        Backend::Scenario::ScenarioDocument *doc,
        const Backend::Scenario::FleetSpec  &fleet);

    // ---------------- undo-restore helpers ----------------

    /// Re-insert a previously captured Connection snapshot. Used by undo
    /// of DeleteConnectionCommand. Returns false if @p doc is null or if
    /// a connection with the same (fromId, toId, mode) already exists.
    static bool restoreConnection(
        Backend::Scenario::ScenarioDocument   *doc,
        const Backend::Scenario::Connection   &snapshot);

    /// Re-insert a previously captured GlobalLink snapshot. Used by undo
    /// of DeleteGlobalLinkCommand. Returns false if @p doc is null or if
    /// a link with the same (fromId, toId, mode) already exists.
    static bool restoreGlobalLink(
        Backend::Scenario::ScenarioDocument   *doc,
        const Backend::Scenario::GlobalLink   &snapshot);

    /// Re-insert a previously captured NodeLinkage snapshot. Used by undo
    /// of UnlinkTerminalCommand. Returns false if @p doc is null or an
    /// identical linkage already exists.
    static bool restoreLinkage(
        Backend::Scenario::ScenarioDocument   *doc,
        const Backend::Scenario::NodeLinkage  &snapshot);

    /// Re-insert a previously captured TerminalPlacement snapshot. Used by
    /// undo of DeleteTerminalCommand. Returns false if @p doc is null, the
    /// placement id is empty, or a terminal with that id already exists.
    static bool restoreTerminal(
        Backend::Scenario::ScenarioDocument        *doc,
        const Backend::Scenario::TerminalPlacement &snapshot);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
