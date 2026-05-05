#pragma once

#include "PathMetrics.h"
#include "Backend/Models/PathSegment.h"

#include <QJsonObject>
#include <QList>
#include <QVariantMap>

namespace CargoNetSim
{
namespace Backend
{
class Path;
namespace Scenario
{

struct EstimatedPathCost
{
    double edgeCost = 0.0;
    double terminalCost = 0.0;
    double totalCost = 0.0;
    PathMetrics metrics;
    QList<PathSegment::SegmentCostSnapshot> segmentCosts;
    QJsonObject costBreakdown;
};

namespace EstimatedPathCostCalculator
{

EstimatedPathCost compute(
    const CargoNetSim::Backend::Path &path,
    const QVariantMap                &costFunctionWeights,
    const QVariantMap                &transportModes,
    int                               containerCount);

PathSegment::SegmentCostSnapshot computeSegmentCost(
    const CargoNetSim::Backend::PathSegment &segment,
    const QVariantMap                       &costFunctionWeights,
    const QVariantMap                       &transportModes,
    int                                      containerCount);

} // namespace EstimatedPathCostCalculator
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
