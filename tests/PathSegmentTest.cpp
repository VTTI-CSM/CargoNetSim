#include <QtTest>
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Path.h"
#include "Backend/Commons/TransportationMode.h"

using CargoNetSim::Backend::PathSegment;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

class PathSegmentTest : public QObject
{
    Q_OBJECT

private slots:
    // After setEstimatedPhysicalMetrics, all three new accessors return
    // the values that were passed in.
    void estimatedPhysicsRoundTrip()
    {
        PathSegment seg("seg1", "T1", "T2", Mode::Ship);
        seg.setEstimatedPhysicalMetrics(100.0, 0.5, 0.03);
        QCOMPARE(seg.estimatedEnergyConsumption(), 100.0);
        QCOMPARE(seg.estimatedCarbonEmissions(),     0.5);
        QCOMPARE(seg.estimatedRisk(),               0.03);
    }

    // Both setters write into the same "estimated" sub-object — they must not
    // overwrite each other. Call in reverse pipeline order (physics first,
    // geometry second) to test the read-modify-write contract hardest.
    void estimatedSettersCoexist()
    {
        PathSegment seg("seg1", "T1", "T2", Mode::Train);
        seg.setEstimatedPhysicalMetrics(200.0, 1.2, 0.06);
        seg.setEstimatedDistanceAndTravelTime(5000.0, 120.0);
        QCOMPARE(seg.estimatedDistance(),            5000.0);
        QCOMPARE(seg.estimatedTravelTime(),           120.0);
        QCOMPARE(seg.estimatedEnergyConsumption(),    200.0);
        QCOMPARE(seg.estimatedCarbonEmissions(),        1.2);
        QCOMPARE(seg.estimatedRisk(),                  0.06);
    }

    // Before any setter, all estimated accessors return 0.
    void estimatedDefaultsAreZero()
    {
        PathSegment seg("seg1", "T1", "T2", Mode::Truck);
        QCOMPARE(seg.estimatedEnergyConsumption(), 0.0);
        QCOMPARE(seg.estimatedCarbonEmissions(),   0.0);
        QCOMPARE(seg.estimatedRisk(),              0.0);
    }

    // Path::totalEstimated* must sum across all segments.
    void pathTotalEstimatedMetrics()
    {
        using CargoNetSim::Backend::Path;

        auto *s1 = new PathSegment("s1", "T1", "T2", Mode::Ship);
        s1->setEstimatedPhysicalMetrics(100.0, 0.5, 0.02);

        auto *s2 = new PathSegment("s2", "T2", "T3", Mode::Train);
        s2->setEstimatedPhysicalMetrics(200.0, 0.8, 0.03);

        Path p(1, 0.0, 0.0, 0.0, QList<CargoNetSim::Backend::Terminal *>{},
                QList<PathSegment *>{s1, s2});

        QCOMPARE(p.totalEstimatedEnergyConsumption(), 300.0);
        QCOMPARE(p.totalEstimatedCarbonEmissions(),     1.3);
        QCOMPARE(p.totalEstimatedRisk(),               0.05);
    }
};

QTEST_APPLESS_MAIN(PathSegmentTest)
#include "PathSegmentTest.moc"
