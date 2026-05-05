#pragma once

#include <QPointF>

namespace CargoNetSim
{
namespace Backend
{
namespace Commons
{
namespace GeoProjection
{

/// Project WGS84 longitude/latitude degrees to EPSG:3857 metres.
/// Returns NaN coordinates when the input cannot be represented.
QPointF wgs84ToWebMercatorMeters(const QPointF &lonLat);

/// Inverse of wgs84ToWebMercatorMeters.
/// Returns NaN coordinates when the input cannot be represented.
QPointF webMercatorMetersToWgs84(const QPointF &meters);

} // namespace GeoProjection
} // namespace Commons
} // namespace Backend
} // namespace CargoNetSim
