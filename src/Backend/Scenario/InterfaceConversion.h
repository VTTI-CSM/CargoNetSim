#pragma once

#include "Backend/Commons/TerminalInterface.h"
#include "Backend/Commons/TransportationMode.h"
#include <QMap>
#include <QPair>
#include <QSet>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Translates between scenario-level interface strings ("Truck", "Rail",
/// "Ship") and the backend enum map Backend::Terminal expects.
///
/// Mirrors the algorithm currently duplicated in
/// GUI/Utils/PathFindingWorker.cpp:382-452. Plan 4 will refactor that
/// call site to delegate here.
namespace InterfaceConversion
{

using InterfaceMap =
    QMap<TerminalTypes::TerminalInterface,
         QSet<TransportationTypes::TransportationMode>>;

/// landStrings members ⊆ {"Truck", "Rail"}; anything else is skipped.
/// seaStrings  members ⊆ {"Ship"};           anything else is skipped.
InterfaceMap toBackendInterfaces(const QStringList &landStrings,
                                 const QStringList &seaStrings);

/// Inverse: recover the {land, sea} string lists from a backend map.
/// Output strings are the canonical forms "Truck"/"Rail"/"Ship".
QPair<QStringList, QStringList>
fromBackendInterfaces(const InterfaceMap &map);

} // namespace InterfaceConversion

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
