#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "LatLon.h"
#include "Point2D.h"
#include "SystemDynamicsSpec.h"
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// A terminal's identity, type, location, and (optional) interface override.
struct TerminalPlacement
{
    /// How the terminal is positioned on the region scene.
    enum class PositionMode
    {
        NetworkNode = 0,  ///< Bound to node N of a named network.
        LatLon      = 1,  ///< Geographic coordinate.
        Scene       = 2,  ///< Raw scene pixel coordinate.
    };

    /// Terminal scenario role — authoring convenience, NOT the runtime
    /// source of truth for origin/destination detection.
    /// isOrigin() checks initial_container_count; isDestination()
    /// checks routing references.
    enum class TerminalRole
    {
        Transit     = 0,  ///< Waypoint (default).
        Origin      = 1,  ///< Container source.
        Destination = 2,  ///< Container sink.
    };

    /// Optional per-terminal interface override. `isSet == false` means "use
    /// type-based defaults"; `isSet == true` with empty sets means "this
    /// terminal explicitly has no interfaces on that side".
    ///
    /// Plan 7 (2026-04-13): migrated from QList<QString> ("Truck"/"Rail"/
    /// "Ship") to typed QSet<TransportationMode>. YAML schema + property-bag
    /// string form are preserved at the boundaries via
    /// interfaceModeCanonicalString / interfaceModeFromCanonicalString.
    struct InterfaceSet
    {
        QSet<TransportationTypes::TransportationMode> landSide;  ///< ⊂ {Truck, Train}
        QSet<TransportationTypes::TransportationMode> seaSide;   ///< ⊂ {Ship}
        bool                                           isSet = false;
    };

    QString id;         ///< Globally unique across the scenario.
    QString type;       ///< One of TerminalTypeDefaults::allTypes().
    QString region;
    TerminalRole role = TerminalRole::Transit;

    /// Terminal-local unstructured config (mirrors TerminalItem property bag
    /// keys — "cost", "dwell_time", "capacity", "customs"). System-dynamics
    /// params have a TYPED home on `systemDynamics` below, NOT in this map,
    /// to avoid dual sources of truth.
    QMap<QString, QVariant> properties;

    PositionMode mode = PositionMode::NetworkNode;
    QString networkRef;                   ///< used when mode == NetworkNode
    int     nodeId  = -1;                 ///< used when mode == NetworkNode
    Point2D scenePos;                     ///< used when mode == Scene
    LatLon  latLon;                       ///< used when mode == LatLon

    InterfaceSet interfaces;

    /// Optional per-terminal system-dynamics override. `systemDynamics.enabled
    /// == false` (the default) is the "omitted" sentinel and is serialized
    /// as the YAML/JSON `system_dynamics` block being absent entirely.
    SystemDynamicsSpec systemDynamics;
};

/// Named `positionModeToString` (not `toString`) to avoid Qt's
/// `QTest::toString` ADL hijack — see LinkageStrategy.h for the full rationale.
QString positionModeToString(TerminalPlacement::PositionMode m);
TerminalPlacement::PositionMode positionModeFromString(const QString &s,
                                                       bool *ok = nullptr);

QString roleToString(TerminalPlacement::TerminalRole r);
TerminalPlacement::TerminalRole roleFromString(
    const QString &s, bool *ok = nullptr);

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
