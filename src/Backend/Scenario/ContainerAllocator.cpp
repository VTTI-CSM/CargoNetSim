// src/Backend/Scenario/ContainerAllocator.cpp
#include "ContainerAllocator.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/Path.h"
#include "ScenarioDocument.h"

#include <QDebug>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
namespace ContainerAllocator {

PathAllocation allocate(const ScenarioDocument &doc,
                        const QList<Path *>    &paths)
{
    qCDebug(lcScenario) << "ContainerAllocator::allocate:"
                        << "origin count =" << doc.originTerminalIds().size()
                        << ", path count =" << paths.size();

    PathAllocation out;

    // Index paths by (origin, destination, rank) so we can locate
    // rank-0 for a given (origin, destination) pair in O(1).
    QHash<PathKey, Path *> pathByKey;
    QHash<QString, QHash<QString, int>> rankCounter;
    for (auto *p : paths)
    {
        if (!p) continue;
        const QString o =
            !p->getOriginId().isEmpty() ? p->getOriginId()
                                        : p->getStartTerminal();
        const QString d =
            !p->getDestinationId().isEmpty()
                ? p->getDestinationId()
                : p->getEndTerminal();
        const int r = p->getPathUid().isEmpty()
                          ? rankCounter[o][d]++
                          : p->getRank();
        PathKey key{ o, d, r };
        pathByKey.insert(key, p);
        out.keyByCanonicalPath.insert(p->canonicalPathKey(), key);
        out.effectiveContainerCountByCanonicalPath.insert(
            p->canonicalPathKey(), 0);
    }

    for (const QString &originId : doc.originTerminalIds())
    {
        const auto allRoutes = doc.destinationsFor(originId);
        if (allRoutes.isEmpty()) continue;

        const auto &pool = doc.containersAt(originId);
        const int   N    = pool.size();
        if (N == 0)
        {
            qCWarning(lcScenario) << "ContainerAllocator::allocate:"
                                  << "origin" << originId
                                  << "has routes but 0 containers";
            continue;
        }

        // Filter to reachable destinations — those with a rank-0 path.
        // Unreachable destinations' share is redistributed across the
        // reachable subset proportionally (re-normalized LRM).
        QList<DestinationRoute> routes;
        double reachableFractionSum = 0.0;
        for (const auto &r : allRoutes)
        {
            const PathKey key{ originId, r.terminal, 0 };
            if (pathByKey.contains(key))
            {
                routes.append(r);
                reachableFractionSum += r.fraction;
            }
            else
            {
                qCWarning(lcScenario) << "ContainerAllocator: no rank-0 path for"
                           << originId << "->" << r.terminal
                           << "— redistributing its"
                           << (r.fraction * 100) << "% share";
            }
        }
        if (routes.isEmpty())
        {
            qCWarning(lcScenario) << "ContainerAllocator: no reachable destinations"
                       << "for origin" << originId
                       << "—" << N << "containers unassigned";
            continue;
        }

        // Re-normalize fractions over the reachable subset so they sum
        // to 1.0. When all routes are reachable, reachableFractionSum
        // ~ 1.0 and this is effectively a no-op.
        QList<double> fractions;
        fractions.reserve(routes.size());
        for (const auto &r : routes)
            fractions.append(r.fraction / reachableFractionSum);

        // Largest-remainder method — integer counts that sum to N.
        QList<int>    counts;
        QList<double> remainders;
        int assigned = 0;
        for (double f : fractions)
        {
            const double exact = N * f;
            const int    base  = static_cast<int>(exact);
            counts.append(base);
            remainders.append(exact - base);
            assigned += base;
        }
        for (int leftover = N - assigned; leftover > 0; --leftover)
        {
            int bestIdx = 0;
            for (int i = 1; i < remainders.size(); ++i)
                if (remainders[i] > remainders[bestIdx]) bestIdx = i;
            counts[bestIdx]    += 1;
            remainders[bestIdx] = -1.0;
        }

        // Assign containers. Every route in `routes` is guaranteed
        // reachable (filtered above) — no silent drops.
        int cursor = 0;
        int assignedForOrigin = 0;
        for (int i = 0; i < routes.size() && cursor < N; ++i)
        {
            const PathKey key{ originId, routes[i].terminal, 0 };
            auto it = pathByKey.constFind(key);
            Q_ASSERT(it != pathByKey.constEnd());

            QList<ContainerCore::Container *> share;
            share.reserve(counts[i]);
            for (int j = 0; j < counts[i] && cursor < N; ++j, ++cursor)
                share.append(pool[cursor]);
            out.byCanonicalPath[(*it)->canonicalPathKey()] =
                share;
            out.effectiveContainerCountByCanonicalPath[(*it)->canonicalPathKey()] =
                share.size();
            assignedForOrigin += share.size();
        }

        qCDebug(lcScenario) << "ContainerAllocator::allocate:"
                            << "origin" << originId
                            << "-> assigned" << assignedForOrigin
                            << "of" << N << "containers across"
                            << routes.size() << "destinations";

        if (assignedForOrigin < N)
            qCWarning(lcScenario) << "ContainerAllocator::allocate:"
                                  << "origin" << originId << ":"
                                  << (N - assignedForOrigin)
                                  << "containers unallocated";
    }

    int totalAllocated = 0;
    for (auto it = out.byCanonicalPath.constBegin();
         it != out.byCanonicalPath.constEnd(); ++it)
        totalAllocated += it.value().size();
    qCInfo(lcScenario) << "ContainerAllocator::allocate:"
                       << "completed — total containers allocated ="
                       << totalAllocated;

    return out;
}

} // namespace ContainerAllocator
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
