#include <QGraphicsScene>
#include <QPixmap>
#include <QTest>

#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

namespace {

struct Harness
{
    QPixmap            px{16, 16};
    QGraphicsScene     scene;
    RegionSpec         region;
    TerminalPlacement  placement;
    RegionCenterPoint *rcp = nullptr;
    TerminalItem      *ti  = nullptr;

    Harness()
    {
        px.fill(Qt::red);
        region.name           = "USA";
        region.color          = "#336699";
        region.localOrigin    = {10.0, 20.0};  // lat, lon
        region.globalPosition = {30.0, 40.0};

        placement.id     = "T1";
        placement.type   = "Origin";
        placement.region = "USA";
        placement.mode   = TerminalPlacement::PositionMode::LatLon;
        placement.latLon = {15.0, 25.0};  // lat, lon

        rcp = new RegionCenterPoint(region.name,
                                    QColor(region.color));
        rcp->setRegionSpecModel(&region);
        scene.addItem(rcp);

        ti = new TerminalItem(px, {}, "USA", nullptr, "Origin");
        ti->setPlacement(&placement);
        scene.addItem(ti);
    }
};

} // namespace

class GlobalTerminalItemViewTest : public QObject
{
    Q_OBJECT
private slots:
    void test_compute_returns_nullopt_without_linked_terminal()
    {
        QPixmap px(16, 16); px.fill(Qt::red);
        GlobalTerminalItem gti(px, nullptr);
        QVERIFY(!gti.computeGlobalLatLon().has_value());
    }

    void test_compute_returns_nullopt_when_terminal_has_no_placement()
    {
        QGraphicsScene scene;
        QPixmap        px(16, 16); px.fill(Qt::red);
        TerminalItem   ti(px, {}, "R", nullptr, "Origin");
        // No setPlacement → placement() returns null.
        scene.addItem(&ti);
        GlobalTerminalItem gti(px, &ti);
        scene.addItem(&gti);
        QVERIFY(!gti.computeGlobalLatLon().has_value());
        // scene takes ownership only if we addItem to it — remove so the
        // stack-allocated ti doesn't get double-deleted.
        scene.removeItem(&ti);
        scene.removeItem(&gti);
    }

    void test_compute_applies_region_offset_when_all_bindings_present()
    {
        Harness h;
        GlobalTerminalItem gti(h.px, h.ti);
        h.scene.addItem(&gti);

        const auto latLon = gti.computeGlobalLatLon();
        QVERIFY(latLon.has_value());
        // Formula: global = globalPosition + (latLon - localOrigin).
        //   lat: 30.0 + (15.0 - 10.0) = 35.0
        //   lon: 40.0 + (25.0 - 20.0) = 45.0
        // QPointF returned is (lon, lat).
        QCOMPARE(latLon->x(), 45.0);
        QCOMPARE(latLon->y(), 35.0);

        h.scene.removeItem(&gti);
    }

    void test_terminal_id_delegates_to_linked_terminal()
    {
        QPixmap            px(16, 16); px.fill(Qt::red);
        // No link → empty domain id; getID still returns a UUID.
        GlobalTerminalItem gtiUnbound(px, nullptr);
        QVERIFY(gtiUnbound.getTerminalId().isEmpty());
        QVERIFY(!gtiUnbound.getID().isEmpty());
        QCOMPARE(gtiUnbound.sceneRegistryKey(), gtiUnbound.getID());

        TerminalPlacement p;
        p.id = "T42"; p.type = "Origin"; p.region = "R";
        TerminalItem      *ti = new TerminalItem(px, {}, "R",
                                                 nullptr, "Origin");
        ti->setPlacement(&p);
        GlobalTerminalItem gtiBound(px, ti);
        // Bound → getTerminalId mirrors the linked item's terminal id.
        QCOMPARE(gtiBound.getTerminalId(), QString("T42"));
        QCOMPARE(gtiBound.sceneRegistryKey(), QString("T42"));
        QVERIFY(gtiBound.getID() != QString("T42"));
        delete ti;
    }

    void test_compute_nullopt_when_no_matching_region_center_in_scene()
    {
        QPixmap       px(16, 16); px.fill(Qt::red);
        TerminalPlacement p;
        p.id = "T1"; p.type = "Origin"; p.region = "USA";
        p.mode = TerminalPlacement::PositionMode::LatLon;

        QGraphicsScene scene;
        TerminalItem  *ti = new TerminalItem(px, {}, "USA",
                                             nullptr, "Origin");
        ti->setPlacement(&p);
        scene.addItem(ti);
        GlobalTerminalItem gti(px, ti);
        scene.addItem(&gti);

        // No RegionCenterPoint in the scene → compute returns nullopt.
        QVERIFY(!gti.computeGlobalLatLon().has_value());

        scene.removeItem(&gti);
    }
};

QTEST_MAIN(GlobalTerminalItemViewTest)
#include "GlobalTerminalItemViewTest.moc"
