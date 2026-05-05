// src/Backend/Scenario/ContainerAllocator.h
#pragma once

#include <QList>

#include "ExecutionPlanTypes.h"
#include "PathAllocation.h"

namespace CargoNetSim {
namespace Backend {

class Path;  // fwd from src/Backend/Models/Path.h

namespace Scenario {

class ScenarioDocument;

/// Policy: given the document's per-origin pools + the discovered
/// paths, return a partition of the pools across paths.
///
/// Demand policy:
///   * AllocatedOnly partitions each origin pool across rank-0 OD paths.
///     Rank >= 1 alternatives remain available for comparison display but
///     receive zero dispatch allocation.
///   * DuplicateDemandPerSelectedPath assigns the full OD demand share to
///     every selected path for that OD. This intentionally duplicates
///     logical demand for alternative-path comparison runs; dispatch still
///     deep-copies containers per vehicle, so simulator objects remain
///     path-scoped.
///
/// Unreachable destinations: if a declared destination has no selected
/// executable path under the active policy, the allocator logs a warning
/// and redistributes the orphaned containers across the remaining
/// reachable destinations via LRM on the reachable subset.
///
/// Invariants the allocator MUST preserve:
///   * Every assigned Container* is a pointer into some
///     `doc.containersAt(originId)` — no new allocation, no deep copy
///     (the segment dispatch factory copies when it dispatches vehicles).
///   * Under AllocatedOnly, no Container* appears in two path buckets.
///     Under DuplicateDemandPerSelectedPath, the same source pointer may
///     appear in multiple selected alternative buckets by design.
///   * Containers from origin O may only appear in paths whose origin
///     is O.
///
/// The allocator is pure: it reads `doc` but does not mutate it.
/// Stateless; all methods are static.
namespace ContainerAllocator
{
    PathAllocation allocate(const ScenarioDocument &doc,
                            const QList<Path *>    &paths,
                            ExecutionDemandPolicy demandPolicy =
                                ExecutionDemandPolicy::AllocatedOnly);
} // namespace ContainerAllocator

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
