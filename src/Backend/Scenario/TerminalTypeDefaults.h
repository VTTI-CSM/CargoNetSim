#pragma once

#include "Backend/Commons/TransportationMode.h"
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Canonical backend-side list of terminal types + their default interface
/// sets + their default property bag. Single source of truth shared by
/// Backend/Scenario/* and GUI/backend authoring flows (TerminalItemFactory,
/// ScenarioEditService::createTerminal). GUI icons live in
/// GUI/Utils/IconCreator.cpp keyed off `allTypes()`.
///
/// Origin/Destination as terminal kinds were removed in Plan 8 — origin
/// role is derived from the property-bag scalar `initial_container_count`
/// (and from the typed `m_containersByTerminal` post-applier), via
/// `ScenarioDocument::isOrigin/isDestination`. Any terminal of any
/// physical kind can be marked as an origin by setting the count > 0.
namespace TerminalTypeDefaults
{

/// The five user-facing physical terminal kinds, in display order.
QStringList allTypes();

/// True if `type` is one of the five canonical strings (case sensitive).
bool isValidType(const QString &type);

/// Returns { land_side, sea_side } default interface mode sets for the given
/// terminal type. Land modes are members of {Truck, Train}; sea modes are
/// {Ship}. Unknown types return a pair of empty sets. Plan 7 migrated this
/// from QStringList to typed enum sets — the property-bag writer
/// (`defaultProperties`) converts to canonical strings at the boundary.
QPair<QSet<TransportationTypes::TransportationMode>,
      QSet<TransportationTypes::TransportationMode>>
interfacesFor(const QString &type);

/// Returns the canonical default property bag for a terminal of the given
/// type. All kinds share: "Name" (placeholder, caller overwrites),
/// "Show on Global Map", "Available Interfaces" (map with "land_side" /
/// "sea_side" QStringList entries derived via `interfacesFor()`), "Region"
/// (empty — caller fills in), "cost" (map), "dwell_time" (map). Sea Port
/// and Intermodal Land additionally carry "customs" + "capacity"; Intermodal
/// / Train Stop / Truck Parking suppress the global-map mirror. Unknown
/// types return an empty map.
QMap<QString, QVariant> defaultProperties(const QString &type);

} // namespace TerminalTypeDefaults

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
