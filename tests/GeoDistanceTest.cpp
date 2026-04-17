#include <QTest>
#include <cmath>

#include "Backend/Commons/GeoDistance.h"

using namespace CargoNetSim::Backend::Commons;

class GeoDistanceTest : public QObject
{
    Q_OBJECT
private slots:
    void test_zero_displacement_is_zero_km()
    {
        QCOMPARE(GeoDistance::haversineKm(40.0, -74.0, 40.0, -74.0), 0.0);
    }

    void test_known_paris_newyork_within_tolerance()
    {
        // Paris (48.8566, 2.3522) ↔ New York (40.7128, -74.0060)
        // Reference great-circle distance ≈ 5837 km.
        const double d = GeoDistance::haversineKm(
            48.8566, 2.3522, 40.7128, -74.0060);
        QVERIFY(std::abs(d - 5837.0) < 30.0);  // ±0.5%
    }

    void test_meters_variant_is_1000x_km()
    {
        const double km = GeoDistance::haversineKm(0.0, 0.0, 0.0, 90.0);
        const double m  = GeoDistance::haversineMeters(0.0, 0.0, 0.0, 90.0);
        QCOMPARE(m, km * 1000.0);
    }

    void test_antipodal_does_not_nan()
    {
        const double d = GeoDistance::haversineKm(0.0, 0.0, 0.0, 180.0);
        QVERIFY(std::isfinite(d));
        QVERIFY(d > 0.0);
    }
};

QTEST_MAIN(GeoDistanceTest)
#include "GeoDistanceTest.moc"
