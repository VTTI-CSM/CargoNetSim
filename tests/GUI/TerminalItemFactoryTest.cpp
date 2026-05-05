#include <QTest>

#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/TerminalItemFactory.h"
#include "GUI/Widgets/GraphicsScene.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class TerminalItemFactoryTest : public QObject
{
    Q_OBJECT
private slots:
    void test_returns_null_on_null_placement()
    {
        GraphicsScene scene;
        QVERIFY(Scenario::TerminalItemFactory::fromPlacement(
                    nullptr, &scene, nullptr)
                == nullptr);
    }

    void test_returns_null_on_null_scene()
    {
        TerminalPlacement p;
        p.id   = "O";
        p.type = "Facility";
        QVERIFY(Scenario::TerminalItemFactory::fromPlacement(
                    &p, nullptr, nullptr)
                == nullptr);
    }

    void test_returns_null_when_pixmap_lookup_fails()
    {
        TerminalPlacement p;
        p.id   = "O";
        p.type = "Not a real terminal kind";
        GraphicsScene scene;
        QVERIFY(Scenario::TerminalItemFactory::fromPlacement(
                    &p, &scene, nullptr)
                == nullptr);
    }

    void test_creates_terminal_from_latlon_placement()
    {
        TerminalPlacement p;
        p.id     = "O";
        p.type   = "Facility";
        p.region = "R";
        p.mode   = TerminalPlacement::PositionMode::LatLon;
        p.latLon = {0.1, 0.2};  // latitude, longitude

        GraphicsScene scene;
        auto *item = Scenario::TerminalItemFactory::fromPlacement(
            &p, &scene, nullptr);

        QVERIFY(item != nullptr);
        QCOMPARE(item->getTerminalType(), QString("Facility"));
        QCOMPARE(item->getRegion(), QString("R"));
        QVERIFY(scene.items().contains(item));
        // Without a MainWindow the factory uses raw (lon, lat).
        QCOMPARE(item->pos(), QPointF(0.2, 0.1));
    }

    void test_creates_terminal_from_scene_placement()
    {
        TerminalPlacement p;
        p.id       = "P";
        p.type     = "Intermodal Land Terminal";
        p.region   = "R";
        p.mode     = TerminalPlacement::PositionMode::Scene;
        p.scenePos = {42.0, 17.0};

        GraphicsScene scene;
        auto *item = Scenario::TerminalItemFactory::fromPlacement(
            &p, &scene, nullptr);

        QVERIFY(item != nullptr);
        QCOMPARE(item->pos(), QPointF(42.0, 17.0));
    }
};

QTEST_MAIN(TerminalItemFactoryTest)
#include "TerminalItemFactoryTest.moc"
