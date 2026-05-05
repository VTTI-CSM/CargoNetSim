/**
 * @file NetworkKind.h
 * @brief Canonical enum classifying a land-network by transport mode.
 */

#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @brief Kind of land network. Ship networks are intentionally absent —
 *        ShipNetSim uses a built-in default world network.
 *
 * This is the single source of truth for "is this network rail or truck?"
 * across the Backend. `Scenario::NetworkSpec::Type` is a type-alias to this
 * enum, keeping the YAML-config layer and the runtime-state layer on one
 * enum without duplicating values.
 */
enum class NetworkKind
{
    Rail  = 0,
    Truck = 1,
};

/// Named `networkKindToString` (not `toString`) to avoid Qt's
/// `QTest::toString` ADL hijack.
QString     networkKindToString(NetworkKind k);
NetworkKind networkKindFromString(const QString &s, bool *ok = nullptr);

} // namespace Backend
} // namespace CargoNetSim
