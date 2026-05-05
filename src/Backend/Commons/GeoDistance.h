#pragma once

namespace CargoNetSim {
namespace Backend {
namespace Commons {

/// Free-function namespace for great-circle distance helpers.
namespace GeoDistance
{
    /// Great-circle distance on a spherical Earth (radius 6371 km).
    /// Inputs in decimal degrees; output in kilometres. Inputs are
    /// clamped to valid asin domain to avoid NaN from floating-point
    /// rounding near antipodes.
    double haversineKm(double lat1, double lon1,
                       double lat2, double lon2);

    /// Same formula, returns metres (convenience for callers that
    /// work in metres per ShortestPathResult convention).
    double haversineMeters(double lat1, double lon1,
                           double lat2, double lon2);
} // namespace GeoDistance

} // namespace Commons
} // namespace Backend
} // namespace CargoNetSim
