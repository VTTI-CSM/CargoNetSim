#include "GeoProjection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace CargoNetSim
{
namespace Backend
{
namespace Commons
{
namespace GeoProjection
{

namespace
{
constexpr double kEarthRadiusMeters = 6378137.0;
constexpr double kMaxMercatorLatitude = 85.051129;

QPointF invalidPoint()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return QPointF(nan, nan);
}

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}
} // namespace

QPointF wgs84ToWebMercatorMeters(const QPointF &lonLat)
{
    if (!isFinitePoint(lonLat))
        return invalidPoint();

    const double lonRad = lonLat.x() * M_PI / 180.0;
    const double lat = std::clamp(lonLat.y(),
                                  -kMaxMercatorLatitude,
                                  kMaxMercatorLatitude);
    const double latRad = lat * M_PI / 180.0;

    const double x = kEarthRadiusMeters * lonRad;
    const double y = kEarthRadiusMeters
                     * std::log(std::tan(M_PI / 4.0 + latRad / 2.0));

    QPointF result(x, y);
    return isFinitePoint(result) ? result : invalidPoint();
}

QPointF webMercatorMetersToWgs84(const QPointF &meters)
{
    if (!isFinitePoint(meters)
        || std::abs(meters.x()) > 1e15
        || std::abs(meters.y()) > 1e15)
    {
        return invalidPoint();
    }

    const double lonRad = meters.x() / kEarthRadiusMeters;
    const double latRad =
        2.0 * std::atan(std::exp(meters.y() / kEarthRadiusMeters))
        - M_PI / 2.0;

    double lonDeg = lonRad * 180.0 / M_PI;
    const double latDeg = latRad * 180.0 / M_PI;
    lonDeg = std::fmod(lonDeg + 180.0, 360.0) - 180.0;

    QPointF result(lonDeg, latDeg);
    return isFinitePoint(result) ? result : invalidPoint();
}

} // namespace GeoProjection
} // namespace Commons
} // namespace Backend
} // namespace CargoNetSim
