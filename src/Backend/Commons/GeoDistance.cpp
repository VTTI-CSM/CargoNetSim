#include "GeoDistance.h"

#include "LogCategories.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace CargoNetSim {
namespace Backend {
namespace Commons {
namespace GeoDistance {

namespace {
constexpr double kEarthRadiusKm = 6371.0;

double toRadians(double d) { return d * M_PI / 180.0; }
} // namespace

double haversineKm(double lat1, double lon1,
                   double lat2, double lon2)
{
    qCDebug(lcModel) << "haversineKm:" << lat1 << lon1
                     << "->" << lat2 << lon2;
    if (std::isnan(lat1) || std::isnan(lon1)
        || std::isnan(lat2) || std::isnan(lon2)
        || std::isinf(lat1) || std::isinf(lon1)
        || std::isinf(lat2) || std::isinf(lon2))
    {
        qCWarning(lcModel) << "haversineKm: NaN/infinite input:"
                           << lat1 << lon1 << lat2 << lon2;
    }
    const double dLat = toRadians(lat2 - lat1);
    const double dLon = toRadians(lon2 - lon1);
    const double a =
        std::sin(dLat / 2) * std::sin(dLat / 2)
      + std::cos(toRadians(lat1)) * std::cos(toRadians(lat2))
      * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double c = 2.0 * std::asin(std::min(1.0, std::sqrt(a)));
    return kEarthRadiusKm * c;
}

double haversineMeters(double lat1, double lon1,
                       double lat2, double lon2)
{
    if (std::isnan(lat1) || std::isnan(lon1)
        || std::isnan(lat2) || std::isnan(lon2)
        || std::isinf(lat1) || std::isinf(lon1)
        || std::isinf(lat2) || std::isinf(lon2))
    {
        qCWarning(lcModel) << "haversineMeters: NaN/infinite input:"
                           << lat1 << lon1 << lat2 << lon2;
    }
    const double result = haversineKm(lat1, lon1, lat2, lon2) * 1000.0;
    qCDebug(lcModel)
        << "haversineMeters(" << lat1 << lon1
        << "->" << lat2 << lon2 << ") =" << result << "m";
    return result;
}

} // namespace GeoDistance
} // namespace Commons
} // namespace Backend
} // namespace CargoNetSim
