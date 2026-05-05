#include <QTest>

#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
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

    void test_bind_resolves_spec_by_name()
    {
        ScenarioDocument doc;
        RegionSpec       r;
        r.name           = "USA";
        r.color          = "#336699";
        r.localOrigin    = {10.0, 20.0};
        r.globalPosition = {30.0, 40.0};
        doc.addRegion(r);

        RegionCenterPoint cp(r.name, QColor(r.color));
        cp.setRegionBinding(&doc, r.name);
        QVERIFY(cp.regionSpecModel() != nullptr);
        QCOMPARE(cp.regionSpecModel()->name, QString("USA"));
        QCOMPARE(cp.regionSpecModel()->localOrigin.latitude, 10.0);
    }

    void test_bind_survives_rename()
    {
        // The whole point of the binding being by-name-not-by-pointer:
        // ScenarioDocument::renameRegion does regions.take()+insert(),
        // which destroys the original QMap node. A raw-pointer binding
        // would now dangle; name-based binding stays valid.
        ScenarioDocument doc;
        RegionSpec       r;
        r.name        = "USA";
        r.color       = "#336699";
        r.localOrigin = {10.0, 20.0};
        doc.addRegion(r);

        RegionCenterPoint cp(r.name, QColor(r.color));
        cp.setRegionBinding(&doc, r.name);

        doc.renameRegion("USA", "Canada");
        cp.setRegion("Canada");  // mirrors the regionRenamed observer

        QVERIFY(cp.regionSpecModel() != nullptr);
        QCOMPARE(cp.regionSpecModel()->name, QString("Canada"));
    }

    void test_unbind_returns_null()
    {
        ScenarioDocument doc;
        RegionSpec       r;
        r.name = "USA";
        doc.addRegion(r);

        RegionCenterPoint cp(r.name, QColor());
        cp.setRegionBinding(&doc, r.name);
        QVERIFY(cp.regionSpecModel() != nullptr);

        cp.setRegionBinding(nullptr, QString());
        QVERIFY(cp.regionSpecModel() == nullptr);
    }
};

QTEST_MAIN(RegionCenterPointViewTest)
#include "RegionCenterPointViewTest.moc"
