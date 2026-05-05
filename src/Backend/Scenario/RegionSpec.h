#pragma once

#include "LatLon.h"
#include "LinkageStrategy.h"
#include "NetworkSpec.h"
#include <QList>
#include <QMap>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Per-region specification: origin anchors, networks, terminal references,
/// and the linkage/connection strategies this region uses.
struct RegionSpec
{
    QString name;
    QString color;           ///< "#RRGGBB"

    LatLon localOrigin;      ///< Maps to RegionCenterPoint "Latitude"/"Longitude"
    LatLon globalPosition;   ///< Maps to RegionCenterPoint "Shared Latitude"/"Shared Longitude"

    QMap<QString, NetworkSpec> networks;  ///< keyed by NetworkSpec::name
    QStringList                terminalIds;

    LinkageStrategy linkageStrategy    = LinkageStrategy::Manual;
    QList<QString>  linkageAutoRules;

    LinkageStrategy connectionStrategy = LinkageStrategy::Manual;
    QList<QString>  connectionAutoRules;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
