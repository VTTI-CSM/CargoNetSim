// tests/UnitsContractTest.cpp
#include <QTest>

#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Commons/Units.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/SimulationTime.h"
#include "Backend/Models/TrainSystem.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/SimulationSettings.h"

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;

class UnitsContractTest : public QObject
{
    Q_OBJECT

private slots:
    void test_route_metric_units_round_trip()
    {
        const QVariantMap display{
            {"travelTime", 1.5},
            {"distance", 12.5},
            {"carbonEmissions", 0.25},
            {"risk", 0.02},
            {"energyConsumption", 48.0},
            {"cost", 99.0}};

        const QVariantMap canonical =
            RouteMetricUnits::canonicalPropertiesFromDisplay(
                display);
        QCOMPARE(canonical.value("travelTime").toDouble(),
                 5400.0);
        QCOMPARE(canonical.value("distance").toDouble(),
                 12500.0);
        QCOMPARE(canonical.value("carbonEmissions").toDouble(),
                 0.25);
        QCOMPARE(canonical.value("risk").toDouble(), 0.02);

        const QVariantMap roundTripped =
            RouteMetricUnits::displayPropertiesFromCanonical(
                canonical);
        QCOMPARE(roundTripped.value("travelTime").toDouble(),
                 1.5);
        QCOMPARE(roundTripped.value("distance").toDouble(),
                 12.5);
    }

    void test_simulation_settings_typed_adapters()
    {
        SimulationSettings settings;
        settings.setTimeStep(
            Units::toSeconds(Units::minutes(15.0)));
        settings.setEndTime(
            Units::toSeconds(Units::hours(24.0)));
        settings.ship.setSpeed(
            Units::kilometersPerHour(30.0));
        settings.ship.setRisk(Units::scalar(0.025));

        QVERIFY(settings.timeStep.has_value());
        QVERIFY(settings.endTime.has_value());
        QVERIFY(settings.ship.speed.has_value());
        QVERIFY(settings.ship.risk.has_value());
        QCOMPARE(settings.timeStep.value(), 900);
        QCOMPARE(settings.endTime.value(), 86400.0);
        QCOMPARE(settings.timeStepUnits()->value(), 900.0);
        QCOMPARE(settings.endTimeUnits()->value(), 86400.0);
        QCOMPARE(settings.ship.speedUnits()->value(), 30.0);
        QCOMPARE(settings.ship.riskUnits()->value(), 0.025);
    }

    void test_shortest_path_result_unit_helpers()
    {
        ShortestPathResult result;
        result.setTotalLength(Units::meters(1250.0));
        result.setMinTravelTime(Units::seconds(450.0));

        QCOMPARE(result.totalLength, 1250.0);
        QCOMPARE(result.minTravelTime, 450.0);
        QCOMPARE(result.totalLengthUnits().value(), 1250.0);
        QCOMPARE(result.minTravelTimeUnits().value(), 450.0);
    }

    void test_model_unit_wrappers()
    {
        auto almostEqual = [](double actual, double expected)
        {
            return qAbs(actual - expected) < 1e-4;
        };

        CargoNetSim::Backend::Locomotive locomotive;
        locomotive.setLength(18.5f);
        locomotive.setGrossWeight(120.0f);
        QVERIFY(almostEqual(locomotive.lengthUnits().value(),
                            18.5));
        QVERIFY(almostEqual(
            locomotive.grossWeightUnits().value(), 120.0));

        CargoNetSim::Backend::Car car;
        car.setLength(14.0f);
        car.setTareWeight(25.0f);
        car.setGrossWeight(80.0f);
        QVERIFY(almostEqual(car.lengthUnits().value(), 14.0));
        QVERIFY(
            almostEqual(car.tareWeightUnits().value(), 25.0));
        QVERIFY(almostEqual(car.grossWeightUnits().value(),
                            80.0));

        CargoNetSim::Backend::Train train;
        train.setLoadTime(1.5f);
        QVERIFY(
            almostEqual(train.loadTimeUnits().value(), 1.5));

        CargoNetSim::Backend::Ship ship;
        ship.setMaxSpeed(22.5f);
        ship.setWaterlineLength(210.0f);
        ship.setBeam(32.2f);
        ship.setDraftAtForward(9.1f);
        ship.setDraftAtAft(10.3f);
        ship.setPropellerDiameter(6.5f);
        ship.setPropellerPitch(7.1f);
        ship.setVesselWeight(45000.0f);
        ship.setCargoWeight(15000.0f);
        ship.setMaxRudderAngle(35.0f);
        QVERIFY(almostEqual(ship.maxSpeedUnits().value(), 22.5));
        QVERIFY(almostEqual(
            ship.waterlineLengthUnits().value(), 210.0));
        QVERIFY(almostEqual(ship.beamUnits().value(), 32.2));
        QVERIFY(almostEqual(ship.draftAtForwardUnits().value(),
                            9.1));
        QVERIFY(almostEqual(ship.draftAtAftUnits().value(),
                            10.3));
        QVERIFY(almostEqual(
            ship.propellerDiameterUnits().value(), 6.5));
        QVERIFY(almostEqual(ship.propellerPitchUnits().value(),
                            7.1));
        QVERIFY(almostEqual(ship.vesselWeightUnits().value(),
                            45000.0));
        QVERIFY(almostEqual(ship.cargoWeightUnits().value(),
                            15000.0));
        QVERIFY(almostEqual(
            ship.maxRudderAngleUnits().value(), 35.0));

        CargoNetSim::Backend::SimulationTime simulationTime(
            90.0);
        QVERIFY(almostEqual(simulationTime.timeStepUnits().value(),
                            90.0));
        simulationTime.advanceByTimeStep();
        QVERIFY(almostEqual(
            simulationTime.currentTimeUnits().value(), 90.0));
        simulationTime.setTimeStepUnits(Units::seconds(120.0));
        QVERIFY(almostEqual(simulationTime.timeStepUnits().value(),
                            120.0));

        CargoNetSim::Backend::PathSegment::SegmentMetricSnapshot
            snapshot;
        snapshot.travelTime = 600.0;
        snapshot.distance = 1250.0;
        snapshot.energyConsumption = 48.0;
        snapshot.carbonEmissions = 0.75;
        snapshot.risk = 0.02;
        QVERIFY(almostEqual(snapshot.travelTimeUnits().value(),
                            600.0));
        QVERIFY(almostEqual(snapshot.distanceUnits().value(),
                            1250.0));
        QVERIFY(almostEqual(
            snapshot.energyConsumptionUnits().value(), 48.0));
        QVERIFY(almostEqual(
            snapshot.carbonEmissionsUnits().value(), 0.75));
        QVERIFY(almostEqual(snapshot.riskUnits().value(), 0.02));
    }
};

QTEST_MAIN(UnitsContractTest)
#include "UnitsContractTest.moc"
