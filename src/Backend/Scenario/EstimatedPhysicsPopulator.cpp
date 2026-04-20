#include "EstimatedPhysicsPopulator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/SegmentPhysicsEstimator.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

EstimatedPhysicsPopulator::EstimatedPhysicsPopulator(ConfigController *config)
    : m_config(config)
{
    Q_ASSERT(config);
}

void EstimatedPhysicsPopulator::populate(
    Path *path, const ScenarioDocument &document) const
{
    if (!path)
        return;

    const int containerCount = resolveContainerCount(path, document);

    const QVariantMap modes      = m_config->getTransportModes();
    const QVariantMap fuelEnergy = m_config->getFuelEnergy();
    const QVariantMap fuelCarbon = m_config->getFuelCarbonContent();

    for (auto *seg : path->getSegments())
    {
        if (!seg)
            continue;

        const double distM = seg->estimatedDistance();
        if (distM <= 0.0)
            continue;

        const auto r = SegmentPhysicsEstimator::estimate(
            seg->getMode(), distM, containerCount,
            modes, fuelEnergy, fuelCarbon);

        seg->setEstimatedPhysicalMetrics(r.energyKWh, r.carbonTonnes, r.risk);
    }
}

int EstimatedPhysicsPopulator::resolveContainerCount(
    const Path *path, const ScenarioDocument &document) const
{
    const auto segs = path->getSegments();
    if (segs.isEmpty())
        return 0;

    const QString originId = segs.first()->getStart();
    const QString destId   = segs.last()->getEnd();

    const int total = document.originContainerCount(originId);
    if (total <= 0)
        return 0;

    const auto routes = document.destinationsFor(originId);
    for (const auto &r : routes)
    {
        if (r.terminal == destId)
            return qRound(total * r.fraction);
    }

    qCDebug(lcScenario)
        << "EstimatedPhysicsPopulator: destination" << destId
        << "not in destinations for" << originId
        << "— using full container count" << total;
    return total;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
