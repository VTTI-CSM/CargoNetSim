#include <QTest>

#include "Backend/Scenario/RegionSpec.h"
#include "GUI/Items/RegionCenterPoint.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class RegionCenterPointViewTest : public QObject
{
    Q_OBJECT
private slots:
    void test_unbound_region_spec_is_null()
    {
        RegionCenterPoint cp("R", QColor(0x33, 0x66, 0x99));
        QCOMPARE(cp.regionSpecModel(),
                 static_cast<RegionSpec *>(nullptr));
    }

    void test_bind_and_unbind_region_spec()
    {
        RegionSpec r;
        r.name           = "USA";
        r.color          = "#336699";
        r.localOrigin    = {10.0, 20.0};
        r.globalPosition = {30.0, 40.0};

        RegionCenterPoint cp(r.name, QColor(r.color));
        cp.setRegionSpecModel(&r);
        QCOMPARE(cp.regionSpecModel(), &r);
        QCOMPARE(cp.regionSpecModel()->name, QString("USA"));
        QCOMPARE(cp.regionSpecModel()->localOrigin.latitude, 10.0);

        cp.setRegionSpecModel(nullptr);
        QVERIFY(cp.regionSpecModel() == nullptr);
    }
};

QTEST_MAIN(RegionCenterPointViewTest)
#include "RegionCenterPointViewTest.moc"
