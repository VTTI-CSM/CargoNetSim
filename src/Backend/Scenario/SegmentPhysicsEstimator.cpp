#include "SegmentPhysicsEstimator.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Scenario/PropertyKeys.h"

#include <QtMath>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

SegmentPhysicsEstimator::Result SegmentPhysicsEstimator::estimate(
    TransportationTypes::TransportationMode mode,
    double                                  distanceMetres,
    int                                     containerCount,
    const QVariantMap                      &transportModes,
    const QVariantMap                      &fuelEnergy,
    const QVariantMap                      &fuelCarbonContent)
{
    if (distanceMetres <= 0.0 || containerCount <= 0)
        return {};

    using M = TransportationTypes::TransportationMode;
    QString modeKey;
    switch (mode)
    {
        case M::Ship:  modeKey = QStringLiteral("ship");  break;
        case M::Train: modeKey = QStringLiteral("rail");  break;
        case M::Truck: modeKey = QStringLiteral("truck"); break;
        default:
            return {};
    }

    const QVariantMap modeProps    = transportModes.value(modeKey).toMap();
    const double fuelRateLperKm    = modeProps.value(PK::Mode::AverageFuelConsumption, 0.0).toDouble();
    const int vehicleCapacity      = qMax(1, modeProps.value(PK::Mode::AverageContainerNumber, 1).toInt());
    const double riskFactor        = modeProps.value(PK::Mode::RiskFactor, 0.0).toDouble();
    const QString fuelType         = modeProps.value(PK::Mode::FuelType).toString();

    if (!fuelEnergy.contains(fuelType) || !fuelCarbonContent.contains(fuelType))
    {
        qCWarning(lcScenario)
            << "SegmentPhysicsEstimator: unknown fuel type" << fuelType
            << "for mode" << modeKey << "— returning zero estimate";
        return {};
    }

    const double fuelEnergyKWhPerL = fuelEnergy.value(fuelType).toDouble();
    const double fuelCarbonKgPerL  = fuelCarbonContent.value(fuelType).toDouble();
    const auto distanceKm =
        Units::LengthMeters(distanceMetres)
            .convert<units::length::kilometer>();
    const int vehicleCount =
        static_cast<int>(qCeil(static_cast<double>(containerCount) / vehicleCapacity));

    const double fuelLitres =
        fuelRateLperKm * distanceKm.value() * vehicleCount;

    Result r;
    r.energyKWh    = fuelLitres * fuelEnergyKWhPerL;
    r.carbonTonnes =
        Units::MassKilograms(fuelLitres * fuelCarbonKgPerL)
            .convert<units::mass::metric_ton>()
            .value();
    r.risk         = riskFactor * vehicleCount;
    return r;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
