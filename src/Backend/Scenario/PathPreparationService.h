#pragma once

#include <QHash>
#include <QList>
#include <QVector>
#include <QString>

#include <memory>
#include <vector>

#include "PreparedPathStatus.h"
#include "Backend/Models/PathSegment.h"
#include "PathKey.h"
#include "PathMetrics.h"

namespace CargoNetSim
{
namespace Backend
{

class ConfigController;
class NetworkController;
class Path;
class RegionDataController;

namespace Scenario
{

class ScenarioDocument;
class ScenarioRegistry;

struct PreparedPathRecord
{
    QString                                     executionPathKey;
    QString                                     canonicalPathKey;
    PreparedPathRequirements                    requirements;
    std::shared_ptr<CargoNetSim::Backend::Path> path;
    PathMetrics                                 predictedMetrics;
    QList<CargoNetSim::Backend::PathSegment::SegmentCostSnapshot>
        predictedSegmentCosts;
};

class PreparedPathSet
{
public:
    int  size() const;
    bool isEmpty() const;

    const std::vector<PreparedPathRecord> &records() const;
    QVector<QString>                       executionPathKeys() const;
    bool                                   containsExecutionPathKey(
        const QString &executionPathKey) const;
    QList<CargoNetSim::Backend::Path *>    rawPaths() const;
    QList<CargoNetSim::Backend::Path *>    rawPaths(
           const QVector<QString> &executionPathKeys) const;

    const QHash<QString, PathMetrics> &predictedMetricsByExecutionPathKey() const;
    const QHash<QString, PathMetrics> &predictedMetricsByCanonicalPath() const;
    const QHash<QString, PathKey>     &pathKeysByExecutionPathKey() const;
    const QHash<QString, PathKey>     &pathKeysByCanonicalPath() const;

private:
    friend class PathPreparationService;

    std::vector<PreparedPathRecord>             m_records;
    QHash<QString, std::shared_ptr<CargoNetSim::Backend::Path>>
        m_pathsByExecutionPathKey;
    QHash<QString, PathMetrics> m_predictedByExecutionPathKey;
    QHash<QString, PathMetrics> m_predictedByCanonicalPath;
    QHash<QString, PathKey>     m_pathKeysByExecutionPathKey;
    QHash<QString, PathKey>     m_pathKeysByCanonicalPath;
};

class PathPreparationService
{
public:
    static PreparedPathSet prepareDiscoveredPaths(
        QList<CargoNetSim::Backend::Path *>        discoveredPaths,
        const ScenarioDocument                    &doc,
        ConfigController                          *config,
        NetworkController                         *networks,
        RegionDataController                      *regionData);

    static PreparedPathSet discoverAndPreparePaths(
        const ScenarioDocument                    &doc,
        const ScenarioRegistry                    &registry,
        int                                        topN,
        ConfigController                          *config,
        NetworkController                         *networks,
        RegionDataController                      *regionData,
        QString                                   *err = nullptr);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
