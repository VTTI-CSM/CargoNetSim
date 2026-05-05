// src/Backend/Scenario/PathKey.h
#pragma once

#include <QHash>
#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {

/// Addresses a discovered path uniquely. PathDiscovery returns one
/// Path* per (origin, destination, rank) tuple — rank is 0-based and
/// distinguishes the k-shortest alternatives within a pair.
struct PathKey
{
    QString originId;
    QString destinationId;
    int     rank = 0;

    bool operator==(const PathKey &o) const
    {
        return originId == o.originId
            && destinationId == o.destinationId
            && rank == o.rank;
    }
};

inline size_t qHash(const PathKey &k, size_t seed = 0) noexcept
{
    return qHashMulti(seed, k.originId, k.destinationId, k.rank);
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
