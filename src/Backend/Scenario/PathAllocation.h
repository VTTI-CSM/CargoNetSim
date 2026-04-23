// src/Backend/Scenario/PathAllocation.h
#pragma once

#include <QHash>
#include <QList>

#include "Backend/Models/Path.h"
#include "PathKey.h"

// Forward-declare ContainerCore::Container — full definition only needed
// where the pointers are dereferenced.
namespace ContainerCore { class Container; }

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Result of ContainerAllocator::allocate. Non-owning: the
/// Container* values are still owned by ScenarioDocument's per-
/// terminal pools. The allocation is a view that says "these
/// containers are assigned to this path for dispatch." The
/// SimulationRequestBuilder consumes the view by deep-copying each
/// assigned container per-segment (the segment builders copy +
/// rewrite IDs internally).
///
/// Rank-0 only: paths with rank >= 1 will NOT appear in
/// `byCanonicalPath` (they carry zero containers). They DO appear in
/// `keyByCanonicalPath` for display purposes. See ContainerAllocator.h
/// for the policy rationale.
struct PathAllocation
{
    QHash<QString /*canonicalPathKey*/,
          QList<ContainerCore::Container *>> byCanonicalPath;
    QHash<QString /*canonicalPathKey*/, PathKey> keyByCanonicalPath;
    QHash<QString /*canonicalPathKey*/, int> effectiveContainerCountByCanonicalPath;

    QList<ContainerCore::Container *>
    containersForPath(const CargoNetSim::Backend::Path *path) const;

    int effectiveContainerCountForPath(
        const CargoNetSim::Backend::Path *path) const;

    PathKey keyForPath(
        const CargoNetSim::Backend::Path *path) const;
};

inline QList<ContainerCore::Container *>
PathAllocation::containersForPath(
    const CargoNetSim::Backend::Path *path) const
{
    if (!path)
        return {};
    return byCanonicalPath.value(path->canonicalPathKey());
}

inline int PathAllocation::effectiveContainerCountForPath(
    const CargoNetSim::Backend::Path *path) const
{
    if (!path)
        return 0;
    return effectiveContainerCountByCanonicalPath.value(
        path->canonicalPathKey(), 0);
}

inline PathKey PathAllocation::keyForPath(
    const CargoNetSim::Backend::Path *path) const
{
    if (!path)
        return {};
    return keyByCanonicalPath.value(path->canonicalPathKey());
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
