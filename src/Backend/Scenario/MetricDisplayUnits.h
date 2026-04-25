#pragma once

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace MetricDisplayUnits
{

inline double distanceKmFromMetres(double distanceMetres)
{
    return distanceMetres / 1000.0;
}

inline double travelTimeHoursFromSeconds(double timeSeconds)
{
    return timeSeconds / 3600.0;
}

} // namespace MetricDisplayUnits
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
