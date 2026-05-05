#pragma once

#include <units.h>

namespace CargoNetSim
{
namespace Backend
{
namespace Units
{

using TimeSeconds = units::time::second_t;
using TimeMinutes = units::time::minute_t;
using TimeHours = units::time::hour_t;
using TimeDays = units::time::day_t;

using LengthMeters = units::length::meter_t;
using LengthKilometers = units::length::kilometer_t;
using LengthMillimeters = units::length::millimeter_t;

using SpeedMetersPerSecond = units::velocity::meters_per_second_t;
using SpeedKilometersPerHour = units::velocity::kilometers_per_hour_t;
using SpeedKnots = units::velocity::knot_t;

using EnergyKilowattHours = units::energy::kilowatt_hour_t;

using MassKilograms = units::mass::kilogram_t;
using MassMetricTons = units::mass::metric_ton_t;

using AreaSquareMeters = units::area::square_meter_t;
using VolumeCubicMeters = units::volume::cubic_meter_t;
using AngleDegrees = units::angle::degree_t;

using Scalar = units::dimensionless::scalar_t;

inline TimeSeconds seconds(double value)
{
    return TimeSeconds(value);
}

inline TimeMinutes minutes(double value)
{
    return TimeMinutes(value);
}

inline TimeHours hours(double value)
{
    return TimeHours(value);
}

inline TimeDays days(double value)
{
    return TimeDays(value);
}

inline LengthMeters meters(double value)
{
    return LengthMeters(value);
}

inline LengthMillimeters millimeters(double value)
{
    return LengthMillimeters(value);
}

inline LengthKilometers kilometers(double value)
{
    return LengthKilometers(value);
}

inline SpeedMetersPerSecond metersPerSecond(double value)
{
    return SpeedMetersPerSecond(value);
}

inline SpeedKilometersPerHour kilometersPerHour(double value)
{
    return SpeedKilometersPerHour(value);
}

inline SpeedKnots knots(double value)
{
    return SpeedKnots(value);
}

inline EnergyKilowattHours kilowattHours(double value)
{
    return EnergyKilowattHours(value);
}

inline MassKilograms kilograms(double value)
{
    return MassKilograms(value);
}

inline MassMetricTons metricTons(double value)
{
    return MassMetricTons(value);
}

inline AreaSquareMeters squareMeters(double value)
{
    return AreaSquareMeters(value);
}

inline VolumeCubicMeters cubicMeters(double value)
{
    return VolumeCubicMeters(value);
}

inline AngleDegrees degrees(double value)
{
    return AngleDegrees(value);
}

inline Scalar scalar(double value)
{
    return Scalar(value);
}

inline TimeSeconds toSeconds(const TimeHours &value)
{
    return value.convert<units::time::second>();
}

inline TimeSeconds toSeconds(const TimeMinutes &value)
{
    return value.convert<units::time::second>();
}

inline TimeSeconds toSeconds(const TimeDays &value)
{
    return value.convert<units::time::second>();
}

inline TimeMinutes toMinutes(const TimeSeconds &value)
{
    return value.convert<units::time::minute>();
}

inline TimeHours toHours(const TimeSeconds &value)
{
    return value.convert<units::time::hour>();
}

inline LengthMeters toMeters(const LengthKilometers &value)
{
    return value.convert<units::length::meter>();
}

inline LengthKilometers toKilometers(const LengthMeters &value)
{
    return value.convert<units::length::kilometer>();
}

inline SpeedMetersPerSecond toMetersPerSecond(
    const SpeedKilometersPerHour &value)
{
    return value.convert<units::velocity::meters_per_second>();
}

inline SpeedKilometersPerHour toKilometersPerHour(
    const SpeedMetersPerSecond &value)
{
    return value.convert<units::velocity::kilometers_per_hour>();
}

inline MassMetricTons toMetricTons(const MassKilograms &value)
{
    return value.convert<units::mass::metric_ton>();
}

} // namespace Units
} // namespace Backend
} // namespace CargoNetSim
