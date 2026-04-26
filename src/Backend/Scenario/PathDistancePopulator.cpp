// src/Backend/Scenario/PathDistancePopulator.cpp
#include "PathDistancePopulator.h"

#include "Backend/Commons/GeoDistance.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/NetworkController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "PropertyKeys.h"

#include <QPointF>
#include <algorithm>
#include <cmath>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

namespace PK = PropertyKeys;

namespace PathDistancePopulator {

namespace {

using Mode = TransportationTypes::TransportationMode;

/// Resolve a terminal -> network node id for the given network type.
/// Returns the first matching linkage or -1.
int nodeIdFor(const ScenarioDocument &doc,
              const QString          &terminalId,
              NetworkSpec::Type       netType)
{
    const auto links = doc.linkagesFor(terminalId, netType);
    return links.isEmpty() ? -1 : links.first().nodeId;
}

QString networkNameFor(const ScenarioDocument &doc,
                       const QString          &terminalId,
                       NetworkSpec::Type       netType)
{
    const auto links = doc.linkagesFor(terminalId, netType);
    return links.isEmpty() ? QString() : links.first().networkName;
}

QString regionOf(const ScenarioDocument &doc,
                 const QString          &terminalId)
{
    return doc.terminals.value(terminalId).region;
}

bool populateRailOrTruck(
    PathSegment                          *seg,
    const ScenarioDocument               &doc,
    const NetworkController              &networks,
    NetworkSpec::Type                     netType)
{
    const QString region = regionOf(doc, seg->getStart());
    const QString net    = networkNameFor(doc, seg->getStart(), netType);
    const int     a      = nodeIdFor(doc, seg->getStart(), netType);
    const int     b      = nodeIdFor(doc, seg->getEnd(),   netType);
    if (net.isEmpty() || a < 0 || b < 0)
    {
        qCWarning(lcScenario) << "PathDistancePopulator::populateRailOrTruck:"
                              << "missing network data for segment"
                              << seg->getStart() << "->" << seg->getEnd()
                              << "(net=" << net << ", nodeA=" << a
                              << ", nodeB=" << b << ")";
        return false;
    }

    ShortestPathResult r;
    if (netType == NetworkSpec::Type::Rail)
    {
        auto *g = networks.trainNetwork(net, region);
        if (!g)
        {
            qCWarning(lcScenario) << "PathDistancePopulator::populateRailOrTruck:"
                                  << "rail network" << net
                                  << "not found in region" << region;
            return false;
        }
        r = g->findShortestPath(a, b, QStringLiteral("distance"));
    }
    else  // Truck
    {
        auto *g = networks.truckNetwork(net, region);
        if (!g)
        {
            qCWarning(lcScenario) << "PathDistancePopulator::populateRailOrTruck:"
                                  << "truck network" << net
                                  << "not found in region" << region;
            return false;
        }
        r = g->findShortestPath(a, b);
    }
    if (r.totalLength <= 0.0) return false;

    qCDebug(lcScenario) << "PathDistancePopulator::populateRailOrTruck:"
                        << seg->getStart() << "->" << seg->getEnd()
                        << "distance=" << r.totalLength << "m"
                        << "time=" << r.minTravelTime << "s";
    seg->setEstimatedDistanceAndTravelTime(r.totalLength, r.minTravelTime);
    return true;
}

bool populateShip(PathSegment                 *seg,
                  const ScenarioDocument      &doc,
                  const ConfigController      &config)
{
    qCDebug(lcScenario) << "PathDistancePopulator::populateShip:"
                        << seg->getStart() << "->" << seg->getEnd();
    const QPointF a = doc.globalPositionOf(seg->getStart());
    const QPointF b = doc.globalPositionOf(seg->getEnd());
    if (!std::isfinite(a.x()) || !std::isfinite(b.x()))
    {
        qCWarning(lcScenario) << "PathDistancePopulator::populateShip:"
                              << "non-finite coordinates for segment"
                              << seg->getStart() << "->" << seg->getEnd();
        return false;
    }

    // QPointF convention: x=lon, y=lat (matches document).
    const auto distMeters = Units::meters(
        Commons::GeoDistance::haversineMeters(
            a.y(), a.x(), b.y(), b.x()));
    const auto distKm = Units::toKilometers(distMeters);

    const double avgSpeedKmH =
        config.getTransportModes()
              .value(QStringLiteral("ship")).toMap()
              .value(PK::Mode::AverageSpeed, 30.0)
              .toDouble();
    const auto travelTimeSeconds = Units::toSeconds(
        Units::hours(distKm.value()
                     / std::max(avgSpeedKmH, 0.01)));

    seg->setEstimatedDistanceAndTravelTime(
        distMeters.value(), travelTimeSeconds.value());
    return true;
}

} // namespace

int populate(const QList<Path *>                &paths,
             const ScenarioDocument             &doc,
             const NetworkController            &networks,
             const ConfigController             &config,
             const RegionDataController         * /*regionData*/)
{
    qCDebug(lcScenario) << "PathDistancePopulator::populate:"
                        << "path count =" << paths.size();

    int count = 0;
    for (auto *p : paths)
    {
        if (!p) continue;
        const auto &segments = p->getSegments();
        double totalDistance = 0.0;
        int segOk = 0;
        for (auto *seg : segments)
        {
            if (!seg) continue;
            if (seg->estimatedDistance() > 0.0
                && seg->estimatedTravelTime() > 0.0)
            {
                ++count;
                ++segOk;
                totalDistance += seg->estimatedDistance();
                continue;
            }
            bool ok = false;
            switch (seg->getMode())
            {
            case Mode::Train:
                ok = populateRailOrTruck(seg, doc, networks,
                                          NetworkSpec::Type::Rail);
                break;
            case Mode::Truck:
                ok = populateRailOrTruck(seg, doc, networks,
                                          NetworkSpec::Type::Truck);
                break;
            case Mode::Ship:
                ok = populateShip(seg, doc, config);
                break;
            default:
                break;
            }
            if (ok)
            {
                ++count;
                ++segOk;
                totalDistance += seg->estimatedDistance();
            }
        }
        qCDebug(lcScenario) << "PathDistancePopulator::populate:"
                            << "path" << p->getPathId()
                            << "-> segments =" << segments.size()
                            << ", populated =" << segOk
                            << ", total distance =" << totalDistance;
    }
    return count;
}

} // namespace PathDistancePopulator
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
