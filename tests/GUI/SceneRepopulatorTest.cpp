#include <QCoreApplication>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/SceneRepopulator.h"
#include "GUI/Widgets/GraphicsScene.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

namespace {

int countOfType(QGraphicsScene &scene, const char *typeidName)
{
    int n = 0;
    for (auto *item : scene.items())
        if (typeid(*item).name() == QString(typeidName)) ++n;
    return n;
}

} // namespace

class SceneRepopulatorTest : public QObject
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

    void test_null_doc_is_noop()
    {
        GraphicsScene region, global;
        Scenario::SceneRepopulator::repopulate(
            nullptr, &region, &global, nullptr);
        QVERIFY(region.items().isEmpty());
        QVERIFY(global.items().isEmpty());
    }

    void test_null_region_scene_is_noop()
    {
        ScenarioDocument doc;
        // Must not crash, must not touch anything.
        Scenario::SceneRepopulator::repopulate(
            &doc, nullptr, nullptr, nullptr);
        // No observable assertions — we're just verifying the early return.
    }

    void test_empty_document_produces_empty_scene()
    {
        ScenarioDocument doc;
        GraphicsScene    region, global;
        Scenario::SceneRepopulator::repopulate(
            &doc, &region, &global, nullptr);
        QVERIFY(region.items().isEmpty());
        QVERIFY(global.items().isEmpty());
    }

    void test_populates_region_centers_and_terminals()
    {
        // 1 region + 2 terminals (no networks → no MapPoints).
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; r.color = "#336699";
        QVERIFY(doc.addRegion(r));

        TerminalPlacement a;
        a.id   = "A"; a.type = "Sea Port Terminal"; a.region = "USA";
        a.mode = TerminalPlacement::PositionMode::LatLon;
        a.latLon = {10.0, 20.0};
        TerminalPlacement b;
        b.id   = "B"; b.type = "Sea Port Terminal"; b.region = "USA";
        b.mode = TerminalPlacement::PositionMode::LatLon;
        b.latLon = {11.0, 21.0};
        QVERIFY(doc.addTerminal(a));
        QVERIFY(doc.addTerminal(b));

        GraphicsScene region, global;
        Scenario::SceneRepopulator::repopulate(
            &doc, &region, &global, nullptr);

        // 1 RegionCenterPoint, 2 TerminalItems — exact counts.
        QCOMPARE(countOfType(region,
                             typeid(RegionCenterPoint).name()), 1);
        QCOMPARE(countOfType(region, typeid(TerminalItem).name()), 2);
    }

    void test_multi_region_pipeline_rebuilds_full_scene()
    {
        // CLI/GUI parity invariant: the same ScenarioDocument, routed through
        // SceneRepopulator, produces a scene whose items each carry a pointer
        // back to the exact document entity a CLI consumer would read.
        //
        // Scope: regions + terminals + regional connections.
        //   - NodeLinkage / MapPoint coverage would require live RegionData
        //     with populated networks (heavy fixture) — covered elsewhere.
        //   - Global-link rebuild requires GlobalTerminalItems in the global
        //     scene, which the production path creates via MainWindow-driven
        //     factory wiring. Coverage lives in ConnectionLineFactoryTest
        //     (test_from_global_link_finds_endpoints_via_linked_terminal_id).
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        ScenarioDocument doc;

        RegionSpec r1; r1.name = "R1"; r1.color = "#336699";
        QVERIFY(doc.addRegion(r1));
        RegionSpec r2; r2.name = "R2"; r2.color = "#993366";
        QVERIFY(doc.addRegion(r2));

        auto makeTerm = [](const QString &id, const QString &region,
                           double lat, double lon) {
            TerminalPlacement t;
            t.id = id; t.type = "Sea Port Terminal"; t.region = region;
            t.mode = TerminalPlacement::PositionMode::LatLon;
            t.latLon = { lat, lon };
            return t;
        };
        const auto a1 = makeTerm("R1-A", "R1", 10.0, 20.0);
        const auto b1 = makeTerm("R1-B", "R1", 11.0, 21.0);
        const auto a2 = makeTerm("R2-A", "R2", 30.0, 40.0);
        const auto b2 = makeTerm("R2-B", "R2", 31.0, 41.0);
        QVERIFY(doc.addTerminal(a1)); QVERIFY(doc.addTerminal(b1));
        QVERIFY(doc.addTerminal(a2)); QVERIFY(doc.addTerminal(b2));

        Connection c1;
        c1.fromTerminalId = "R1-A"; c1.toTerminalId = "R1-B";
        c1.mode = Mode::Ship; c1.region = "R1";
        Connection c2;
        c2.fromTerminalId = "R2-A"; c2.toTerminalId = "R2-B";
        c2.mode = Mode::Ship; c2.region = "R2";
        QVERIFY(doc.addConnection(c1));
        QVERIFY(doc.addConnection(c2));

        GraphicsScene region, global;
        Scenario::SceneRepopulator::repopulate(
            &doc, &region, &global, nullptr);

        // Exact counts: no leaks, no duplicates.
        QCOMPARE(countOfType(region, typeid(RegionCenterPoint).name()), 2);
        QCOMPARE(countOfType(region, typeid(TerminalItem).name()),      4);
        QCOMPARE(countOfType(region, typeid(ConnectionLine).name()),    2);
        QVERIFY(global.items().isEmpty());

        // View-model bindings — the core CLI/GUI parity claim.
        // Every terminal view resolves to exactly one document entity.
        for (const QString &id : { "R1-A", "R1-B", "R2-A", "R2-B" })
        {
            auto *ti = region.getItemById<TerminalItem>(id);
            QVERIFY2(ti != nullptr, qPrintable("missing " + id));
            QCOMPARE(ti->placement(), &doc.terminals[id]);
        }

        // Each ConnectionLine carries a non-null Connection* pointing into
        // doc.connections — observer round-trip wired model to view.
        const auto lines = region.getItemsByType<ConnectionLine>();
        QCOMPARE(lines.size(), 2);
        QSet<const Connection *> seen;
        for (auto *line : lines)
        {
            auto *m = line->connectionModel();
            QVERIFY(m != nullptr);
            QVERIFY(m == &doc.connections[0] || m == &doc.connections[1]);
            seen.insert(m);
        }
        QCOMPARE(seen.size(), 2);
    }

    void test_repopulate_is_idempotent()
    {
        // Running repopulate twice must produce the same scene as running
        // it once — tears down via clearAll first.
        ScenarioDocument doc;
        RegionSpec r; r.name = "R"; r.color = "#888888";
        doc.addRegion(r);
        TerminalPlacement t;
        t.id = "T"; t.type = "Origin"; t.region = "R";
        t.mode = TerminalPlacement::PositionMode::LatLon;
        doc.addTerminal(t);

        GraphicsScene region, global;
        Scenario::SceneRepopulator::repopulate(
            &doc, &region, &global, nullptr);
        const int firstCount = region.items().count();

        Scenario::SceneRepopulator::repopulate(
            &doc, &region, &global, nullptr);
        QCOMPARE(region.items().count(), firstCount);
    }
};

QTEST_MAIN(SceneRepopulatorTest)
#include "SceneRepopulatorTest.moc"
