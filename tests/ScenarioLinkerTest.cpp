#include <QCoreApplication>
#include <QDir>
#include <QTest>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/ScenarioApplier.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioLinker.h"
#include "Backend/Scenario/ScenarioRegistry.h"

class ScenarioLinkerTest : public QObject
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

    void test_linker_rule_names_contain_initial_set()
    {
        const auto linkageRules = CargoNetSim::Backend::Scenario::ScenarioLinker::linkageRuleNames();
        QVERIFY(linkageRules.contains("land_terminal_to_rail_node"));
        QVERIFY(linkageRules.contains("truck_parking_to_truck_node"));
        QVERIFY(linkageRules.contains("sea_port_to_nearest_rail"));

        const auto connRules = CargoNetSim::Backend::Scenario::ScenarioLinker::connectionRuleNames();
        QVERIFY(connRules.contains("by_networks"));
        QVERIFY(connRules.contains("by_interfaces"));

        const auto globalRules = CargoNetSim::Backend::Scenario::ScenarioLinker::globalRuleNames();
        QVERIFY(globalRules.contains("sea_port_pairs_within_km"));
    }

    void test_rule_land_terminal_to_rail_node_picks_nearest_rail_node()
    {
        using namespace CargoNetSim::Backend::Scenario;

        // Build a scenario with one region, one rail network (fixture), and
        // one Intermodal Land Terminal positioned near node 0.
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.linkageStrategy = LinkageStrategy::Auto;
        r.linkageAutoRules = { "land_terminal_to_rail_node" };
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "USA_rail";
        rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_nodes.dat");
        rail.files["links"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_links.dat");
        doc.addNetwork("USA", rail);

        TerminalPlacement t;
        t.id = "IL1"; t.type = "Intermodal Land Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        // Place IL1 near the real USA rail fixture's first node (node 1:
        // x=1100978.17 y=2831426.60). Being within kMaxNearestDistance of
        // any node is sufficient — the rule just picks the nearest.
        t.scenePos = { 1100978.0, 2831426.0 };
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto linkages = ScenarioLinker::resolveLinkages(doc, registry);
        // At least one linkage for IL1 pointing at USA_rail.
        bool found = false;
        for (const auto &l : linkages)
        {
            if (l.terminalId == "IL1" && l.networkName == "USA_rail" && l.nodeId == 1)
                found = true;
        }
        QVERIFY(found);
    }

    void test_rule_truck_parking_to_truck_node_picks_nearest_truck_node()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.linkageStrategy = LinkageStrategy::Auto;
        r.linkageAutoRules = { "truck_parking_to_truck_node" };
        doc.addRegion(r);

        NetworkSpec truck;
        truck.name = "USA_truck";
        truck.type = NetworkSpec::Type::Truck;
        truck.files["config"] = QDir(QCoreApplication::applicationDirPath())
                                    .filePath("fixtures/scenario/truck_config.dat");
        doc.addNetwork("USA", truck);

        TerminalPlacement t;
        t.id = "TP1"; t.type = "Truck Parking"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        // truck_nodes.dat has node 1 at (0,0), node 2 at (100,0). Place TP1
        // at origin to pick node 1.
        t.scenePos = { 0.0, 0.0 };
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto linkages = ScenarioLinker::resolveLinkages(doc, registry);
        bool found = false;
        for (const auto &l : linkages)
        {
            if (l.terminalId == "TP1" && l.networkName == "USA_truck" && l.nodeId == 1)
                found = true;
        }
        QVERIFY(found);
    }

    void test_rule_sea_port_to_nearest_rail_picks_rail_node()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.linkageStrategy = LinkageStrategy::Auto;
        r.linkageAutoRules = { "sea_port_to_nearest_rail" };
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "USA_rail";
        rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_nodes.dat");
        rail.files["links"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_links.dat");
        doc.addNetwork("USA", rail);

        TerminalPlacement t;
        t.id = "SP1"; t.type = "Sea Port Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        t.scenePos = { 1100978.0, 2831426.0 };  // near rail node 1
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto linkages = ScenarioLinker::resolveLinkages(doc, registry);
        bool found = false;
        for (const auto &l : linkages)
        {
            if (l.terminalId == "SP1" && l.networkName == "USA_rail" && l.nodeId == 1)
                found = true;
        }
        QVERIFY(found);
    }

    void test_rule_by_networks_connects_terminals_sharing_a_network()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.connectionStrategy = LinkageStrategy::Auto;
        r.connectionAutoRules = { "by_networks" };
        doc.addRegion(r);

        TerminalPlacement a; a.id = "A"; a.type = "Intermodal Land Terminal"; a.region = "USA";
        a.mode = TerminalPlacement::PositionMode::Scene;                doc.addTerminal(a);
        TerminalPlacement b; b.id = "B"; b.type = "Intermodal Land Terminal"; b.region = "USA";
        b.mode = TerminalPlacement::PositionMode::Scene;                doc.addTerminal(b);

        // Manually establish a shared-network linkage.
        NodeLinkage la; la.terminalId = "A"; la.networkName = "USA_rail"; la.nodeId = 0; doc.addLinkage(la);
        NodeLinkage lb; lb.terminalId = "B"; lb.networkName = "USA_rail"; lb.nodeId = 1; doc.addLinkage(lb);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto connections = ScenarioLinker::resolveConnections(doc, registry);
        bool found = false;
        for (const auto &c : connections)
            if ((c.fromTerminalId == "A" && c.toTerminalId == "B") ||
                (c.fromTerminalId == "B" && c.toTerminalId == "A"))
                found = true;
        QVERIFY(found);
    }

    void test_rule_by_interfaces_connects_via_shared_mode()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.connectionStrategy = LinkageStrategy::Auto;
        r.connectionAutoRules = { "by_interfaces" };
        doc.addRegion(r);

        TerminalPlacement a; a.id = "SP_A"; a.type = "Sea Port Terminal"; a.region = "USA";
        a.mode = TerminalPlacement::PositionMode::Scene; doc.addTerminal(a);
        TerminalPlacement b; b.id = "SP_B"; b.type = "Sea Port Terminal"; b.region = "USA";
        b.mode = TerminalPlacement::PositionMode::Scene; doc.addTerminal(b);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto connections = ScenarioLinker::resolveConnections(doc, registry);
        bool sawRail = false;   // Sea Port default has Rail on land_side
        for (const auto &c : connections)
            if (c.mode == CargoNetSim::Backend::TransportationTypes::TransportationMode::Train) sawRail = true;
        QVERIFY(sawRail);
    }

    void test_preview_load_enables_land_terminal_rule_without_apply()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.linkageStrategy = LinkageStrategy::Auto;
        r.linkageAutoRules = { "land_terminal_to_rail_node" };
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "USA_rail"; rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_nodes.dat");
        rail.files["links"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_links.dat");
        doc.addNetwork("USA", rail);

        TerminalPlacement t; t.id = "IL1"; t.type = "Intermodal Land Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        // Place near node 1 (x=1100978.17, y=2831426.60) in the real USA rail fixture.
        t.scenePos = { 1100978.0, 2831426.0 };
        doc.addTerminal(t);

        // NO ScenarioApplier::apply — preview path loads networks directly
        // into the registry without mutating the live controller.
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioLinker::loadNetworksForPreview(doc, registry, &err),
                 qPrintable(err));

        auto result = ScenarioLinker::resolveLinkages(doc, registry);
        bool found = false;
        for (const auto &l : result)
            if (l.terminalId == "IL1" && l.nodeId == 1) found = true;
        QVERIFY(found);
    }

    void test_resolver_hybrid_unions_manual_and_auto_minus_excludes()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        r.linkageStrategy = LinkageStrategy::Hybrid;
        r.linkageAutoRules = { "land_terminal_to_rail_node" };
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "USA_rail"; rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_nodes.dat");
        rail.files["links"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_links.dat");
        doc.addNetwork("USA", rail);

        TerminalPlacement t; t.id = "IL1"; t.type = "Intermodal Land Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        // Place IL1 near node 1 so the auto-rule picks nodeId==1 (not 0).
        t.scenePos = { 1100978.0, 2831426.0 };
        doc.addTerminal(t);

        // A manual linkage to node 99 AND an exclude for node 1 (the auto-rule pick).
        NodeLinkage manual; manual.terminalId = "IL1"; manual.networkName = "USA_rail"; manual.nodeId = 99;
        doc.addLinkage(manual);
        NodeLinkage excl;   excl.terminalId   = "IL1"; excl.networkName   = "USA_rail"; excl.nodeId   = 1;
        excl.excluded = true; doc.addLinkage(excl);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto result = ScenarioLinker::resolveLinkages(doc, registry);
        bool sawManual = false, sawAutoExcluded = false;
        for (const auto &l : result)
        {
            if (l.terminalId == "IL1" && l.nodeId == 99) sawManual = true;
            if (l.terminalId == "IL1" && l.nodeId == 1)  sawAutoExcluded = true;
        }
        QVERIFY( sawManual);
        QVERIFY(!sawAutoExcluded);
    }
};

QTEST_MAIN(ScenarioLinkerTest)
#include "ScenarioLinkerTest.moc"
