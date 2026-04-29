#pragma once

#include "Backend/Models/Path.h"
#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

struct RuntimeArtifactIdentity
{
    QString pathKey;
    int     segmentIndex = -1;
    int     artifactIndex = -1;
    QString artifactType;

    bool isValid() const
    {
        return !pathKey.isEmpty() && segmentIndex >= 0
            && artifactIndex >= 0 && !artifactType.isEmpty();
    }
};

namespace RuntimeArtifacts {

QString encode(const RuntimeArtifactIdentity &id);

bool decode(const QString &encoded,
            RuntimeArtifactIdentity &out);

QString vehicleId(const QString &canonicalPathKey,
                  int            segmentIndex,
                  int            artifactIndex,
                  const QString &artifactType);

QString vehicleId(const Backend::Path *path, int segmentIndex,
                  int artifactIndex,
                  const QString &artifactType);

QString copiedContainerId(
    const QString &canonicalPathKey, int segmentIndex,
    int artifactIndex, const QString &sourceContainerId);

QString copiedContainerId(
    const Backend::Path *path, int segmentIndex,
    int artifactIndex, const QString &sourceContainerId);

} // namespace RuntimeArtifacts

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
