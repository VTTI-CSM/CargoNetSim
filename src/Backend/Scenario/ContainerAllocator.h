// src/Backend/Scenario/ContainerAllocator.h
#pragma once

#include <QList>

#include "PathAllocation.h"

namespace CargoNetSim {
namespace Backend {

class Path;  // fwd from src/Backend/Models/Path.h

namespace Scenario {

class ScenarioDocument;

/// Policy: given the document's per-origin pools + the discovered
/// paths, return a partition of the pools across paths.
///
/// Rank policy: only **rank-0** paths receive allocated containers. When
/// `shortest_paths_n > 1`, PathDiscovery returns multiple paths per
/// (origin, destination) pair; paths with rank >= 1 are kept for
/// comparison display but receive zero dispatch allocation here. This is
/// intentional: the system dispatches all traffic on the best route, not
/// across alternatives. Preview demand metrics for those alternatives are
/// computed separately from this allocator.
///
/// Unreachable destinations: if a declared destination has no
/// discovered rank-0 path, the allocator logs a warning and
/// redistributes the orphaned containers across the remaining
/// reachable destinations via LRM on the reachable subset.
///
/// Invariants the allocator MUST preserve:
///   * Every assigned Container* is a pointer into some
///     `doc.containersAt(originId)` — no new allocation, no deep copy
///     (the builder does the copy when it dispatches per vehicle).
///   * No Container* appears in two path buckets (a container can
///     only flow through one path).
///   * Containers from origin O may only appear in paths whose origin
///     is O.
///
/// The allocator is pure: it reads `doc` but does not mutate it.
/// Stateless; all methods are static.
namespace ContainerAllocator
{
    PathAllocation allocate(const ScenarioDocument &doc,
                            const QList<Path *>    &paths);
} // namespace ContainerAllocator

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
