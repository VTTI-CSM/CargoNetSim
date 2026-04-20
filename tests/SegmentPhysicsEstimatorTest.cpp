#include <QtTest>
#include <cmath>
#include "Backend/Scenario/SegmentPhysicsEstimator.h"
#include "Backend/Commons/TransportationMode.h"

using CargoNetSim::Backend::Scenario::SegmentPhysicsEstimator;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

// Build a minimal transport-modes QVariantMap for the given mode key.
static QVariantMap makeModes(const QString &modeKey,
                              double fuelRate, int capacity,
                              double risk, const QString &fuelType)
{
    QVariantMap modeProps;
    modeProps["average_fuel_consumption"] = fuelRate;
    modeProps["average_container_number"] = capacity;
    modeProps["risk_factor"]              = risk;
    modeProps["fuel_type"]                = fuelType;

    QVariantMap modes;
    modes[modeKey] = modeProps;
    return modes;
}

class SegmentPhysicsEstimatorTest : public QObject
{
    Q_OBJECT

private slots:
    // Ship nominal: 1 vehicle needed, distance = 42.6 km
    void shipNominal()
    {
        const double distM = 42600.0;
        const int    containers = 10;

        QVariantMap modes = makeModes("ship", 21.0, 10000, 0.025, "HFO");
        QVariantMap energy{{"HFO", 11.1}};
        QVariantMap carbon{{"HFO", 3.15}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Ship, distM, containers, modes, energy, carbon);

        const double distKm = 42.6;
        const int    veh    = 1;   // ceil(10/10000)
        const double fuel   = 21.0 * distKm * veh;

        QVERIFY(r.energyKWh    > 0.0);
        QVERIFY(r.carbonTonnes > 0.0);
        QCOMPARE(r.risk, 0.025 * veh);
        QVERIFY(qAbs(r.energyKWh    - fuel * 11.1)        < 1e-9);
        QVERIFY(qAbs(r.carbonTonnes - fuel * 3.15 / 1000) < 1e-9);
    }

    // Vehicle count ceiling: capacity=3, containers=7 → vehicleCount=3
    void vehicleCountCeiling()
    {
        const double distM = 10000.0;   // 10 km
        QVariantMap modes = makeModes("truck", 0.55, 3, 0.012, "diesel_2");
        QVariantMap energy{{"diesel_2", 10.0}};
        QVariantMap carbon{{"diesel_2", 2.68}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Truck, distM, 7, modes, energy, carbon);

        const int    veh  = 3;    // ceil(7/3)
        const double dist = 10.0; // km
        const double fuel = 0.55 * dist * veh;

        QVERIFY(qAbs(r.energyKWh    - fuel * 10.0)        < 1e-9);
        QVERIFY(qAbs(r.carbonTonnes - fuel * 2.68 / 1000) < 1e-9);
        QCOMPARE(r.risk, 0.012 * veh);
    }

    // Missing fuel type → all-zero Result
    void missingFuelType()
    {
        QVariantMap modes = makeModes("ship", 21.0, 10000, 0.025, "UNKNOWN_FUEL");
        QVariantMap energy{{"HFO", 11.1}};
        QVariantMap carbon{{"HFO", 3.15}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Ship, 42600.0, 10, modes, energy, carbon);

        QCOMPARE(r.energyKWh,    0.0);
        QCOMPARE(r.carbonTonnes, 0.0);
        QCOMPARE(r.risk,         0.0);
    }

    // containerCount == 0 → all-zero Result
    void zeroContainers()
    {
        QVariantMap modes = makeModes("rail", 5.5, 180, 0.006, "diesel_1");
        QVariantMap energy{{"diesel_1", 10.7}};
        QVariantMap carbon{{"diesel_1", 2.68}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Train, 50000.0, 0, modes, energy, carbon);

        QCOMPARE(r.energyKWh,    0.0);
        QCOMPARE(r.carbonTonnes, 0.0);
        QCOMPARE(r.risk,         0.0);
    }

    // distanceMetres == 0 → all-zero Result
    void zeroDistance()
    {
        QVariantMap modes = makeModes("rail", 5.5, 180, 0.006, "diesel_1");
        QVariantMap energy{{"diesel_1", 10.7}};
        QVariantMap carbon{{"diesel_1", 2.68}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Train, 0.0, 50, modes, energy, carbon);

        QCOMPARE(r.energyKWh,    0.0);
        QCOMPARE(r.carbonTonnes, 0.0);
        QCOMPARE(r.risk,         0.0);
    }

    // YAML-override custom fuel type
    void customFuelType()
    {
        QVariantMap modes = makeModes("truck", 0.55, 1, 0.012, "bio_diesel");
        QVariantMap energy{{"bio_diesel", 9.5}};
        QVariantMap carbon{{"bio_diesel", 2.0}};

        auto r = SegmentPhysicsEstimator::estimate(
            Mode::Truck, 100000.0, 1, modes, energy, carbon);

        const double fuel = 0.55 * 100.0 * 1;  // 100 km, 1 vehicle
        QVERIFY(qAbs(r.energyKWh    - fuel * 9.5)        < 1e-9);
        QVERIFY(qAbs(r.carbonTonnes - fuel * 2.0 / 1000) < 1e-9);
    }
};

QTEST_APPLESS_MAIN(SegmentPhysicsEstimatorTest)
#include "SegmentPhysicsEstimatorTest.moc"
