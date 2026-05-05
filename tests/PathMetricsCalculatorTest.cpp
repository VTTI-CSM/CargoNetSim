// tests/PathMetricsCalculatorTest.cpp
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/PathMetricsCalculator.h"
#include "Backend/Scenario/PathMetricsInputs.h"

using namespace CargoNetSim::Backend::Scenario;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

namespace {

PathMetricsInputs railInputs(bool useNetwork = false)
{
    PathMetricsInputs in;
    QVariantMap rail;
    rail[QStringLiteral("average_fuel_consumption")] = 2.5;
    rail[QStringLiteral("risk_factor")]              = 0.02;
    rail[QStringLiteral("use_network")]              = useNetwork;
    rail[QStringLiteral("fuel_type")]                = QStringLiteral("diesel_1");
    rail[QStringLiteral("average_speed")]            = 60.0;
    in.modeProperties = rail;
    in.fuelEnergy[QStringLiteral("diesel_1")]        = 10.0;
    in.fuelCarbonContent[QStringLiteral("diesel_1")] = 2.68;
    return in;
}

} // namespace

class PathMetricsCalculatorTest : public QObject
{
    Q_OBJECT
private slots:
    void test_compute_train_non_network_uses_average_speed()
    {
        const auto in = railInputs(/*useNetwork=*/false);
        const auto m = PathMetricsCalculator::compute(
            /*distanceMeters=*/120'000.0,
            /*timeSeconds=*/7200.0,                     // ignored
            Mode::Train, in);

        QVERIFY(m.valid);
        QCOMPARE(m.distanceKm,         120.0);
        QCOMPARE(m.distance().value(), 120.0);
        QCOMPARE(m.travelTimeHours,      2.0);   // 120 / 60
        QCOMPARE(m.travelTime().value(), 2.0);
        QCOMPARE(m.fuelPerVehicle,       2.5);
        QCOMPARE(m.riskPerVehicle,       0.02);
        QCOMPARE(m.riskVehicleUnits().value(), 0.02);
        QCOMPARE(m.fuelType,             QStringLiteral("diesel_1"));
        QCOMPARE(m.energyPerVehicle,  3000.0);   // 2.5 * 120 * 10 * 1
        QCOMPARE(m.energyVehicleUnits().value(), 3000.0);
        QCOMPARE(m.carbonPerVehicle,     0.804); // 2.5 * 120 * 2.68 / 1000
        QCOMPARE(m.carbonVehicleUnits().value(), 0.804);
    }

    void test_compute_train_network_mode_uses_time_seconds()
    {
        const auto in = railInputs(/*useNetwork=*/true);
        const auto m = PathMetricsCalculator::compute(
            120'000.0, 7200.0, Mode::Train, in);
        QCOMPARE(m.travelTimeHours, 2.0);
    }

    void test_compute_ship_always_uses_average_speed()
    {
        PathMetricsInputs in;
        QVariantMap ship;
        ship[QStringLiteral("average_fuel_consumption")] = 10.0;
        ship[QStringLiteral("risk_factor")]              = 0.05;
        ship[QStringLiteral("use_network")]              = true;   // ignored for Ship
        ship[QStringLiteral("average_speed")]            = 30.0;
        in.modeProperties = ship;
        in.fuelEnergy[QStringLiteral("HFO")]         = 12.0;
        in.fuelCarbonContent[QStringLiteral("HFO")]  = 3.2;

        const auto m = PathMetricsCalculator::compute(
            300'000.0, 99999.0, Mode::Ship, in);
        QCOMPARE(m.distanceKm,        300.0);
        QCOMPARE(m.distance().value(), 300.0);
        QCOMPARE(m.travelTimeHours,    10.0);
        QCOMPARE(m.travelTime().value(), 10.0);
        QCOMPARE(m.fuelType,          QStringLiteral("HFO"));
    }

    void test_compute_unknown_fuel_type_falls_back_to_default()
    {
        PathMetricsInputs in;
        QVariantMap truck;
        truck[QStringLiteral("average_fuel_consumption")] = 0.3;
        truck[QStringLiteral("average_speed")]            = 60.0;
        in.modeProperties = truck;
        in.fuelEnergy[QStringLiteral("diesel_2")]        = 10.0;
        in.fuelCarbonContent[QStringLiteral("diesel_2")] = 2.7;

        const auto m = PathMetricsCalculator::compute(
            100'000.0, 6000.0, Mode::Truck, in);
        QCOMPARE(m.fuelType, QStringLiteral("diesel_2"));
        QVERIFY(m.valid);
    }

    void test_compute_empty_inputs_flags_invalid()
    {
        PathMetricsInputs empty;
        const auto m = PathMetricsCalculator::compute(
            1000.0, 60.0, Mode::Train, empty);
        QVERIFY(!m.valid);
    }

    void test_compute_any_mode_flags_invalid()
    {
        const auto in = railInputs();
        const auto m = PathMetricsCalculator::compute(
            1000.0, 60.0, Mode::Any, in);
        QVERIFY(!m.valid);
    }

    void test_compute_with_container_count_populates_per_container()
    {
        auto in = railInputs(/*useNetwork=*/false);
        // Train capacity for per-container projection.
        in.modeProperties[QStringLiteral("average_container_number")] = 300;

        // 120 km, 100 containers, capacity 300 → vehiclesNeeded = 1.
        // per-container = per-vehicle × vehiclesNeeded / containerCount
        const auto m = PathMetricsCalculator::compute(
            /*distanceMeters=*/120'000.0,
            /*timeSeconds=*/7200.0,
            Mode::Train, in,
            /*containerCount=*/100);

        QCOMPARE(m.containerCount,     100);
        QCOMPARE(m.vehiclesNeeded,       1);
        QCOMPARE(m.fuelPerContainer,  0.025);   // 2.5 * 1 / 100
        QCOMPARE(m.energyPerContainer, 30.0);   // 3000 * 1 / 100
    }

    void test_compute_per_vehicle_overload_leaves_per_container_zero()
    {
        const auto in = railInputs();
        const auto m = PathMetricsCalculator::compute(
            10'000.0, 600.0, Mode::Train, in);   // 4-arg overload
        QCOMPARE(m.containerCount,   0);
        QCOMPARE(m.vehiclesNeeded,   0);
        QCOMPARE(m.fuelPerContainer, 0.0);
    }

    void test_compute_container_count_zero_matches_per_vehicle_overload()
    {
        const auto in = railInputs();
        const auto a = PathMetricsCalculator::compute(
            10'000.0, 600.0, Mode::Train, in);
        const auto b = PathMetricsCalculator::compute(
            10'000.0, 600.0, Mode::Train, in, /*containerCount=*/0);
        QCOMPARE(a.distanceKm,        b.distanceKm);
        QCOMPARE(a.distance().value(), b.distance().value());
        QCOMPARE(a.fuelPerVehicle,    b.fuelPerVehicle);
        QCOMPARE(b.fuelPerContainer,  0.0);
    }

    void test_compute_partial_vehicle_increases_per_container_share()
    {
        auto in = railInputs(/*useNetwork=*/false);
        in.modeProperties[QStringLiteral("average_container_number")] = 300;

        // 50 containers on 300-capacity train → 1 vehicle, 1/6 loaded.
        // per-container fuel = 2.5 * 1 / 50 = 0.05 (higher than 100-container case).
        const auto m = PathMetricsCalculator::compute(
            120'000.0, 7200.0, Mode::Train, in, /*containerCount=*/50);
        QCOMPARE(m.vehiclesNeeded,    1);
        QCOMPARE(m.fuelPerContainer,  0.05);
    }
};

QTEST_MAIN(PathMetricsCalculatorTest)
#include "PathMetricsCalculatorTest.moc"
