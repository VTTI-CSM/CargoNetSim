#include "PathPreparationService.h"

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/NetworkController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Models/Path.h"
#include "ContainerAllocator.h"
#include "EstimatedPhysicsPopulator.h"
#include "PathAllocation.h"
#include "PathDemandResolver.h"
#include "PathDiscovery.h"
#include "PathDistancePopulator.h"
#include "PropertyKeys.h"
#include "PreparedPathEligibilityService.h"
#include "EstimatedPathCostCalculator.h"
#include "ScenarioDocument.h"
#include "ScenarioRegistry.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QList<CargoNetSim::Backend::Path *> rawPreparedRecordPaths(
    const std::vector<PreparedPathRecord> &records)
{
    QList<CargoNetSim::Backend::Path *> paths;
    paths.reserve(static_cast<qsizetype>(records.size()));
    for (const auto &record : records)
    {
        if (record.path)
            paths.append(record.path.get());
    }
    return paths;
}

QString basePreparedExecutionPathKey(
    const CargoNetSim::Backend::Path &path)
{
    const QString canonicalPathKey = path.canonicalPathKey();
    if (!canonicalPathKey.isEmpty())
        return canonicalPathKey;

    return QStringLiteral("path_id|%1")
        .arg(path.getPathId());
}

QString makeUniquePreparedExecutionPathKey(
    const QString &baseIdentity,
    const QHash<QString, std::shared_ptr<CargoNetSim::Backend::Path>>
        &existing)
{
    const QString normalizedBase =
        baseIdentity.isEmpty() ? QStringLiteral("path")
                               : baseIdentity;
    QString candidate = normalizedBase;
    int     suffix    = 2;
    while (existing.contains(candidate))
    {
        candidate = QStringLiteral("%1#%2")
                        .arg(normalizedBase)
                        .arg(suffix++);
    }
    return candidate;
}

PathKey pathKeyFor(const CargoNetSim::Backend::Path &path)
{
    PathKey key;
    key.originId =
        path.getOriginId().isEmpty() ? path.getStartTerminal()
                                     : path.getOriginId();
    key.destinationId =
        path.getDestinationId().isEmpty() ? path.getEndTerminal()
                                          : path.getDestinationId();
    key.rank = path.getRank();
    return key;
}

int capacityForMode(const QVariantMap &transportModes,
                    TransportationTypes::TransportationMode mode)
{
    return qMax(
        1,
        transportModes
            .value(transportationModeToString(mode))
            .toMap()
            .value(PropertyKeys::Mode::AverageContainerNumber, 1)
            .toInt());
}

QList<PathMetrics::VehicleRequirement>
previewVehicleBreakdownForPath(
    const CargoNetSim::Backend::Path &path,
    const ConfigController           &config,
    int                               containerCount)
{
    QList<PathMetrics::VehicleRequirement> requirements;
    if (containerCount <= 0)
        return requirements;

    const auto transportModes = config.getTransportModes();
    const auto segments = path.getSegments();
    requirements.reserve(segments.size());
    for (int i = 0; i < segments.size(); ++i)
    {
        const auto *segment = segments.at(i);
        if (!segment)
            continue;

        PathMetrics::VehicleRequirement requirement;
        requirement.segmentIndex =
            segment->sequenceIndex() >= 0 ? segment->sequenceIndex()
                                          : i;
        requirement.mode = segment->getMode();
        const int capacity =
            capacityForMode(transportModes, requirement.mode);
        requirement.vehiclesNeeded =
            qMax(1, (containerCount + capacity - 1) / capacity);
        requirements.append(requirement);
    }
    return requirements;
}

} // namespace

int PreparedPathSet::size() const
{
    return static_cast<int>(m_records.size());
}

bool PreparedPathSet::isEmpty() const
{
    return m_records.empty();
}

const std::vector<PreparedPathRecord> &PreparedPathSet::records() const
{
    return m_records;
}

QVector<QString> PreparedPathSet::executionPathKeys() const
{
    QVector<QString> identities;
    identities.reserve(static_cast<qsizetype>(m_records.size()));
    for (const auto &record : m_records)
    {
        if (!record.executionPathKey.isEmpty())
            identities.append(record.executionPathKey);
    }
    return identities;
}

bool PreparedPathSet::containsExecutionPathKey(
    const QString &executionPathKey) const
{
    return m_pathsByExecutionPathKey.contains(executionPathKey);
}

QList<CargoNetSim::Backend::Path *> PreparedPathSet::rawPaths() const
{
    return rawPreparedRecordPaths(m_records);
}

QList<CargoNetSim::Backend::Path *> PreparedPathSet::rawPaths(
    const QVector<QString> &executionPathKeys) const
{
    QList<CargoNetSim::Backend::Path *> selected;
    selected.reserve(executionPathKeys.size());
    for (const auto &executionPathKey : executionPathKeys)
    {
        const auto path = m_pathsByExecutionPathKey.value(executionPathKey);
        if (path)
            selected.append(path.get());
    }
    return selected;
}

const QHash<QString, PathMetrics> &
PreparedPathSet::predictedMetricsByExecutionPathKey() const
{
    return m_predictedByExecutionPathKey;
}

const QHash<QString, PathMetrics> &
PreparedPathSet::predictedMetricsByCanonicalPath() const
{
    return m_predictedByCanonicalPath;
}

const QHash<QString, PathKey> &
PreparedPathSet::pathKeysByExecutionPathKey() const
{
    return m_pathKeysByExecutionPathKey;
}

const QHash<QString, PathKey> &
PreparedPathSet::pathKeysByCanonicalPath() const
{
    return m_pathKeysByCanonicalPath;
}

PreparedPathSet PathPreparationService::prepareDiscoveredPaths(
    QList<CargoNetSim::Backend::Path *>        discoveredPaths,
    const ScenarioDocument                    &doc,
    ConfigController                          *config,
    NetworkController                         *networks,
    RegionDataController                      *regionData)
{
    PreparedPathSet prepared;
    prepared.m_records.reserve(
        static_cast<size_t>(discoveredPaths.size()));

    for (auto *path : discoveredPaths)
    {
        if (!path)
            continue;
        PreparedPathRecord record;
        record.path = std::shared_ptr<CargoNetSim::Backend::Path>(path);
        record.canonicalPathKey = path->canonicalPathKey();
        record.requirements =
            PreparedPathEligibilityService::requirementsFor(*path);
        record.executionPathKey = makeUniquePreparedExecutionPathKey(
            basePreparedExecutionPathKey(*path),
            prepared.m_pathsByExecutionPathKey);
        prepared.m_pathsByExecutionPathKey.insert(record.executionPathKey,
                                              record.path);
        prepared.m_records.push_back(std::move(record));
    }

    const auto paths = rawPreparedRecordPaths(prepared.m_records);
    if (paths.isEmpty())
        return prepared;

    if (config && networks)
    {
        PathDistancePopulator::populate(paths, doc, *networks,
                                        *config, regionData);
    }

    const auto allocation = ContainerAllocator::allocate(doc, paths);

    for (auto &record : prepared.m_records)
    {
        auto *path = record.path.get();
        if (!path)
            continue;

        path->setEffectiveContainerCount(
            allocation.effectiveContainerCountForPath(path));

        const auto key = pathKeyFor(*path);
        prepared.m_pathKeysByExecutionPathKey.insert(
            record.executionPathKey, key);
        if (!record.canonicalPathKey.isEmpty())
        {
            if (prepared.m_pathKeysByCanonicalPath.contains(
                    record.canonicalPathKey))
            {
                qCWarning(lcScenario)
                    << "PathPreparationService::prepareDiscoveredPaths:"
                    << "duplicate canonical path key"
                    << record.canonicalPathKey
                    << "while building metadata; keeping first entry";
            }
            else
            {
                prepared.m_pathKeysByCanonicalPath.insert(
                    record.canonicalPathKey, key);
            }
        }

        if (!config)
            continue;

        EstimatedPhysicsPopulator physics(config);
        physics.populate(path, doc);
        if (path->getSegments().isEmpty())
            continue;

        const int previewContainerCount =
            PathDemandResolver::previewContainerCount(doc, *path);
        const auto estimatedCost =
            EstimatedPathCostCalculator::compute(
                *path, config->getCostFunctionWeights(),
                config->getTransportModes(),
                previewContainerCount);
        path->setTotalEdgeCosts(estimatedCost.edgeCost);
        path->setTotalTerminalCosts(estimatedCost.terminalCost);
        path->setTotalPathCost(estimatedCost.totalCost);
        path->setRankingCost(estimatedCost.totalCost);
        path->setCostBreakdown(estimatedCost.costBreakdown);
        record.predictedMetrics = estimatedCost.metrics;
        record.predictedSegmentCosts =
            estimatedCost.segmentCosts;
        if (record.predictedMetrics.previewVehicleBreakdown.isEmpty())
        {
            record.predictedMetrics.previewVehicleBreakdown =
                previewVehicleBreakdownForPath(
                    *path, *config, previewContainerCount);
        }
        prepared.m_predictedByExecutionPathKey.insert(
            record.executionPathKey, record.predictedMetrics);
        if (!record.canonicalPathKey.isEmpty())
        {
            if (prepared.m_predictedByCanonicalPath.contains(
                    record.canonicalPathKey))
            {
                qCWarning(lcScenario)
                    << "PathPreparationService::prepareDiscoveredPaths:"
                    << "duplicate canonical path key"
                    << record.canonicalPathKey
                    << "while building predicted metrics; keeping first entry";
            }
            else
            {
                prepared.m_predictedByCanonicalPath.insert(
                    record.canonicalPathKey,
                    record.predictedMetrics);
            }
        }
    }

    qCDebug(lcScenario) << "PathPreparationService::prepareDiscoveredPaths:"
                        << "prepared" << prepared.size()
                        << "path(s)";
    return prepared;
}

PreparedPathSet PathPreparationService::discoverAndPreparePaths(
    const ScenarioDocument                    &doc,
    const ScenarioRegistry                    &registry,
    int                                        topN,
    ConfigController                          *config,
    NetworkController                         *networks,
    RegionDataController                      *regionData,
    QString                                   *err)
{
    PathDiscovery discovery;
    auto paths = discovery.findTopPaths(doc, registry, topN, err);
    return prepareDiscoveredPaths(std::move(paths), doc, config,
                                  networks, regionData);
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
