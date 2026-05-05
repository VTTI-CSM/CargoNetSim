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
/// containers are assigned to this path for execution." Live execution
/// creates path-scoped runtime copies before seeding TerminalSim and before
/// dispatching active segments.
///
/// The demand policy is chosen by ContainerAllocator. In the default
/// operational policy, rank >= 1 alternatives carry zero dispatch demand.
/// In comparison policy, multiple selected alternatives for the same OD
/// may each reference the same source-container view; downstream dispatch
/// creates path-scoped runtime copies before submitting to simulators.
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
