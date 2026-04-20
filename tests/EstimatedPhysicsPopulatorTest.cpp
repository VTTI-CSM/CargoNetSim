#include <QCoreApplication>
#include <QtTest>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/EstimatedPhysicsPopulator.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/SegmentPhysicsEstimator.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Commons/TransportationMode.h"

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;
using Mode = TransportationTypes::TransportationMode;

// Minimal ScenarioDocument: region R1, origin T1 (50 containers, T2 @0.4),
// destination T2.  Caller owns the returned pointer.
static ScenarioDocument *makeDoc()
{
    auto *doc = new ScenarioDocument();

    RegionSpec r;
    r.name = "R1";
    doc->addRegion(r);

    TerminalPlacement t1;
    t1.id     = "T1";
    t1.type   = "port";
    t1.region = "R1";
    t1.role   = TerminalPlacement::TerminalRole::Origin;
    t1.properties["initial_container_count"] = 50;
    t1.properties["destinations"] = QVariantList{
        QVariantMap{ {"terminal", "T2"}, {"fraction", 0.4} }
    };
    doc->addTerminal(t1);

    TerminalPlacement t2;
    t2.id     = "T2";
    t2.type   = "port";
    t2.region = "R1";
    t2.role   = TerminalPlacement::TerminalRole::Destination;
    doc->addTerminal(t2);

    return doc;
}

// Two rail segments T1→MID (50 km) and MID→T2 (30 km) with distances set.
static Path *makePath()
{
    auto *s1 = new PathSegment("s1", "T1", "MID", Mode::Train);
    s1->setEstimatedDistanceAndTravelTime(50000.0, 2040.0);

    auto *s2 = new PathSegment("s2", "MID", "T2", Mode::Train);
    s2->setEstimatedDistanceAndTravelTime(30000.0, 1224.0);

    return new Path(1, 0.0, 0.0, 0.0, {}, {s1, s2});
}

class EstimatedPhysicsPopulatorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
            new CargoNetSim::CargoNetSimController(nullptr, QCoreApplication::instance());
    }

    // populate() writes non-zero energy/carbon/risk on segments with distance.
    // Container count = round(50 × 0.4) = 20.
    void populatesNonZeroValues()
    {
        auto *cfg = CargoNetSim::CargoNetSimController::getInstance().getConfigController();
        EstimatedPhysicsPopulator pop(cfg);
        ScenarioDocument *doc = makeDoc();
        Path *path = makePath();

        pop.populate(path, *doc);

        const auto segs = path->getSegments();
        QVERIFY(segs[0]->estimatedEnergyConsumption() > 0.0);
        QVERIFY(segs[0]->estimatedCarbonEmissions()   > 0.0);
        QVERIFY(segs[0]->estimatedRisk()              > 0.0);
        QVERIFY(segs[1]->estimatedEnergyConsumption() > 0.0);

        delete path;
        delete doc;
    }

    // Path totals equal sum of per-segment values.
    void pathTotalsMatchSegmentSum()
    {
        auto *cfg = CargoNetSim::CargoNetSimController::getInstance().getConfigController();
        EstimatedPhysicsPopulator pop(cfg);
        ScenarioDocument *doc = makeDoc();
        Path *path = makePath();

        pop.populate(path, *doc);

        const auto segs = path->getSegments();
        const double sumE = segs[0]->estimatedEnergyConsumption()
                          + segs[1]->estimatedEnergyConsumption();
        const double sumC = segs[0]->estimatedCarbonEmissions()
                          + segs[1]->estimatedCarbonEmissions();
        QCOMPARE(path->totalEstimatedEnergyConsumption(), sumE);
        QCOMPARE(path->totalEstimatedCarbonEmissions(),   sumC);

        delete path;
        delete doc;
    }

    // Segment with zero distance is skipped — values stay 0.
    void zeroDistanceSegmentSkipped()
    {
        auto *cfg = CargoNetSim::CargoNetSimController::getInstance().getConfigController();
        EstimatedPhysicsPopulator pop(cfg);
        ScenarioDocument *doc = makeDoc();

        auto *s1 = new PathSegment("s1", "T1", "MID", Mode::Train);
        // NO setEstimatedDistanceAndTravelTime → distance stays 0
        auto *s2 = new PathSegment("s2", "MID", "T2", Mode::Train);
        s2->setEstimatedDistanceAndTravelTime(30000.0, 1224.0);

        Path *path = new Path(2, 0.0, 0.0, 0.0, {}, {s1, s2});
        pop.populate(path, *doc);

        QCOMPARE(path->getSegments()[0]->estimatedEnergyConsumption(), 0.0);
        QVERIFY( path->getSegments()[1]->estimatedEnergyConsumption() > 0.0);

        delete path;
        delete doc;
    }

    // Values match SegmentPhysicsEstimator direct call for containerCount=20.
    void valuesMatchEstimatorDirectCall()
    {
        auto *cfg = CargoNetSim::CargoNetSimController::getInstance().getConfigController();
        EstimatedPhysicsPopulator pop(cfg);
        ScenarioDocument *doc = makeDoc();
        Path *path = makePath();

        pop.populate(path, *doc);

        auto expected = SegmentPhysicsEstimator::estimate(
            Mode::Train,
            50000.0,
            20,           // round(50 × 0.4)
            cfg->getTransportModes(),
            cfg->getFuelEnergy(),
            cfg->getFuelCarbonContent());

        const auto *s0 = path->getSegments()[0];
        QVERIFY(qAbs(s0->estimatedEnergyConsumption() - expected.energyKWh)    < 1e-9);
        QVERIFY(qAbs(s0->estimatedCarbonEmissions()   - expected.carbonTonnes) < 1e-9);
        QCOMPARE(s0->estimatedRisk(), expected.risk);

        delete path;
        delete doc;
    }
};

QTEST_MAIN(EstimatedPhysicsPopulatorTest)
#include "EstimatedPhysicsPopulatorTest.moc"
