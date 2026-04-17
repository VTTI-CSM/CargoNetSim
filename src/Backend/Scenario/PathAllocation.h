// src/Backend/Scenario/PathAllocation.h
#pragma once

#include <QHash>
#include <QList>

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
/// Rank-0 only: paths with rank >= 1 will NOT appear in `byPathId`
/// (they carry zero containers). They DO appear in `keyByPathId`
/// for display purposes. See ContainerAllocator.h for the policy
/// rationale.
///
/// Keyed by path id (int) rather than PathKey because PathDiscovery's
/// output is addressed by id everywhere downstream; the PathKey
/// mapping is carried alongside for CLI / GUI display needs.
struct PathAllocation
{
    QHash<int /*pathId*/, QList<ContainerCore::Container *>> byPathId;

    /// Parallel lookup: path id → human-facing (origin, dest, rank)
    /// triple. The allocator populates both in one pass.
    QHash<int /*pathId*/, PathKey> keyByPathId;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
