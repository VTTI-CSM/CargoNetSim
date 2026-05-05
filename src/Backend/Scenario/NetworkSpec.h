#pragma once

#include "Backend/Commons/NetworkKind.h"
#include "Point2D.h"
#include <QMap>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Land-network specification (rail or truck). Ship networks are NOT
/// user-configurable — see design spec §5.2 for rationale.
///
/// The kind enum lives in `Backend::NetworkKind`; `NetworkSpec::Type` is
/// an alias so existing scenario-layer callers (and the YAML `type:` key)
/// read naturally. String round-trip uses `networkKindToString` /
/// `networkKindFromString` directly — no scenario-layer duplication.
struct NetworkSpec
{
    using Type = CargoNetSim::Backend::NetworkKind;

    QString name;
    Type    type = Type::Rail;
    /// Region-local anchor recorded with imported network declarations.
    /// Runtime drawing currently derives node scene positions from the
    /// network data itself; this value is document metadata, not a GUI-only
    /// placement override.
    Point2D referencePoint;

    /// File paths (relative to the scenario YAML's directory, resolved at
    /// parse time). Contract:
    ///   Rail  → {"nodes": ..., "links": ...}
    ///   Truck → {"config": ...}
    QMap<QString, QString> files;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
