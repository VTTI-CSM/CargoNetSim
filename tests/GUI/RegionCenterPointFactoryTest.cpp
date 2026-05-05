#include <QCoreApplication>
#include <QTest>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/RegionSpec.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Scenario/RegionCenterPointFactory.h"
#include "GUI/Widgets/GraphicsScene.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class RegionCenterPointFactoryTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Tier 1: construct the controller explicitly for this test
        // binary. Parent to QCoreApplication::instance() so Qt cleans
        // it up at the end of the binary's life. Guarded so repeated
        // initTestCase calls in the same binary are safe.
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                /*logger=*/nullptr, QCoreApplication::instance());
        }
    }

    void test_returns_null_on_null_region()
    {
        GraphicsScene scene;
        QVERIFY(Scenario::RegionCenterPointFactory::fromRegionSpec(
                    nullptr, &scene, nullptr)
                == nullptr);
    }

    void test_returns_null_on_null_scene()
    {
        RegionSpec region;
        region.name = "R";
        QVERIFY(Scenario::RegionCenterPointFactory::fromRegionSpec(
                    &region, nullptr, nullptr)
                == nullptr);
    }

    void test_constructs_center_point_from_region_spec()
    {
        RegionSpec region;
        region.name           = "USA";
        region.color          = "#336699";
        region.localOrigin    = {10.0, 20.0};   // latitude, longitude
        region.globalPosition = {30.0, 40.0};

        GraphicsScene scene;
        auto *cp = Scenario::RegionCenterPointFactory::fromRegionSpec(
            &region, &scene, nullptr);

        QVERIFY(cp != nullptr);
        QCOMPARE(cp->getRegion(), QString("USA"));
        QVERIFY(scene.items().contains(cp));
    }
};

QTEST_MAIN(RegionCenterPointFactoryTest)
#include "RegionCenterPointFactoryTest.moc"
