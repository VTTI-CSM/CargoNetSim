#pragma once

#include <QHash>
#include <QList>
#include <QVector>
#include <QString>

#include <memory>
#include <vector>

#include "PreparedPathStatus.h"
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
class VehicleController;

namespace Scenario
{

class ScenarioDocument;
class ScenarioRegistry;

struct PreparedPathRecord
{
    QString                                     pathIdentity;
    QString                                     canonicalPathKey;
    PreparedPathRequirements                    requirements;
    std::shared_ptr<CargoNetSim::Backend::Path> path;
    PathMetrics                                 predictedMetrics;
};

class PreparedPathSet
{
public:
    int  size() const;
    bool isEmpty() const;

    const std::vector<PreparedPathRecord> &records() const;
    QVector<QString>                       pathIdentities() const;
    bool                                   containsPathIdentity(
        const QString &pathIdentity) const;
    QList<CargoNetSim::Backend::Path *>    rawPaths() const;
    QList<CargoNetSim::Backend::Path *>    rawPaths(
           const QVector<QString> &pathIdentities) const;

    const QHash<QString, PathMetrics> &predictedMetricsByPathIdentity() const;
    const QHash<QString, PathMetrics> &predictedMetricsByCanonicalPath() const;
    const QHash<QString, PathKey>     &pathKeysByPathIdentity() const;
    const QHash<QString, PathKey>     &pathKeysByCanonicalPath() const;

private:
    friend class PathPreparationService;

    std::vector<PreparedPathRecord>             m_records;
    QHash<QString, std::shared_ptr<CargoNetSim::Backend::Path>>
        m_pathsByPathIdentity;
    QHash<QString, PathMetrics> m_predictedByPathIdentity;
    QHash<QString, PathMetrics> m_predictedByCanonicalPath;
    QHash<QString, PathKey>     m_pathKeysByPathIdentity;
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
        RegionDataController                      *regionData,
        VehicleController                         *vehicles);

    static PreparedPathSet discoverAndPreparePaths(
        const ScenarioDocument                    &doc,
        const ScenarioRegistry                    &registry,
        int                                        topN,
        ConfigController                          *config,
        NetworkController                         *networks,
        RegionDataController                      *regionData,
        VehicleController                         *vehicles,
        QString                                   *err = nullptr);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
