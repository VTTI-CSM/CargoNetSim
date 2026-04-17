#include <QSignalSpy>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "GUI/Scenario/ScenarioMutator.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI::Scenario;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

namespace {

/// Set up a document with a single region so terminals have somewhere to
/// live. Kept local to the test file so each test composes its own state.
void seedRegion(ScenarioDocument &doc, const QString &name)
{
    RegionSpec r;
    r.name = name;
    QVERIFY(doc.addRegion(r));
}

} // namespace

class ScenarioMutatorTest : public QObject
{
    Q_OBJECT
private slots:
    void test_create_terminal_adds_to_document_and_emits_signal()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        QSignalSpy addedSpy(&doc, &ScenarioDocument::terminalAdded);

        const QString id = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R", QPointF(0.2, 0.1));

        QVERIFY(!id.isEmpty());
        QCOMPARE(addedSpy.count(), 1);
        QVERIFY(doc.terminals.contains(id));
        QCOMPARE(doc.terminals[id].type,
                 QString("Sea Port Terminal"));
        QCOMPARE(doc.terminals[id].mode,
                 TerminalPlacement::PositionMode::LatLon);
        // QPointF(x=lon=0.2, y=lat=0.1) → LatLon{latitude=0.1, longitude=0.2}.
        QCOMPARE(doc.terminals[id].latLon.latitude,  0.1);
        QCOMPARE(doc.terminals[id].latLon.longitude, 0.2);
    }

    void test_create_terminal_returns_empty_id_on_null_doc()
    {
        QVERIFY(ScenarioMutator::createTerminal(
                    nullptr, "Sea Port Terminal", "R", QPointF(0, 0))
                    .isEmpty());
    }

    // ---- Plan 8: setTerminalProperty contract (sole property-bag write
    //      path from the GUI; new PropertiesPanel "Origin Configuration"
    //      group routes through here) ----

    void test_set_terminal_property_writes_through_document()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        const QString id = ScenarioMutator::createTerminal(
            &doc, "Intermodal Land Terminal", "R", QPointF(0, 0));
        QVERIFY(!id.isEmpty());

        QSignalSpy changedSpy(&doc, &ScenarioDocument::terminalChanged);
        QVERIFY(ScenarioMutator::setTerminalProperty(
            &doc, id, "initial_container_count", QVariant(42)));

        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(doc.terminals[id]
                     .properties.value("initial_container_count")
                     .toInt(),
                 42);
        // Round-trips through ScenarioDocument::isOrigin without any
        // GUI involvement — CLI/GUI share this rule.
        QVERIFY(doc.isOrigin(id));
    }

    void test_set_terminal_property_clears_when_value_invalid()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        const QString id = ScenarioMutator::createTerminal(
            &doc, "Intermodal Land Terminal", "R", QPointF(0, 0));
        QVERIFY(ScenarioMutator::setTerminalProperty(
            &doc, id, "initial_container_count", QVariant(7)));
        QVERIFY(doc.isOrigin(id));

        // Invalid QVariant → key removed entirely.
        QVERIFY(ScenarioMutator::setTerminalProperty(
            &doc, id, "initial_container_count", QVariant{}));

        QVERIFY(!doc.terminals[id]
                     .properties.contains("initial_container_count"));
        QVERIFY(!doc.isOrigin(id));
    }

    void test_set_terminal_property_returns_false_on_unknown_id()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        QVERIFY(!ScenarioMutator::setTerminalProperty(
            &doc, "missing", "key", QVariant("x")));
    }

    void test_set_terminal_property_returns_false_on_null_doc()
    {
        QVERIFY(!ScenarioMutator::setTerminalProperty(
            nullptr, "id", "key", QVariant("x")));
    }

    void test_link_terminal_to_node_adds_linkage()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        const QString id = ScenarioMutator::createTerminal(
            &doc, "Intermodal Land Terminal", "R", QPointF(0, 0));
        QVERIFY(!id.isEmpty());

        QSignalSpy linkSpy(&doc, &ScenarioDocument::linkageAdded);
        const bool ok = ScenarioMutator::linkTerminalToNode(
            &doc, id, "rail_net_a", 42, LinkageSource::Manual);

        QVERIFY(ok);
        QCOMPARE(linkSpy.count(), 1);
        QCOMPARE(doc.linkages.size(), 1);
        QCOMPARE(doc.linkages.first().networkName, QString("rail_net_a"));
        QCOMPARE(doc.linkages.first().nodeId, 42);
    }

    void test_create_connection_fills_region_from_endpoints()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        const QString a = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R", QPointF(0, 0));
        const QString b = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R", QPointF(1, 1));
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        QSignalSpy connSpy(&doc, &ScenarioDocument::connectionAdded);
        const bool ok = ScenarioMutator::createConnection(
            &doc, a, b, Mode::Ship);

        QVERIFY(ok);
        QCOMPARE(connSpy.count(), 1);
        QCOMPARE(doc.connections.size(), 1);
        // Invariant: region is derived from endpoints, not passed by caller.
        QCOMPARE(doc.connections.first().region, QString("R"));
    }

    void test_create_connection_rejects_cross_region_endpoints()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R1");
        seedRegion(doc, "R2");
        const QString a = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R1", QPointF(0, 0));
        const QString b = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R2", QPointF(1, 1));

        QVERIFY(!ScenarioMutator::createConnection(&doc, a, b, Mode::Ship));
        QVERIFY(doc.connections.isEmpty());
    }

    void test_update_region_local_origin_emits_region_changed()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        QSignalSpy changedSpy(&doc, &ScenarioDocument::regionChanged);

        const bool ok = ScenarioMutator::updateRegionLocalOrigin(
            &doc, "R", QPointF(10.0, 20.0));  // lon=10, lat=20

        QVERIFY(ok);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(doc.regions["R"].localOrigin.latitude,  20.0);
        QCOMPARE(doc.regions["R"].localOrigin.longitude, 10.0);
    }

    void test_create_global_link_requires_both_terminals()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R1");
        seedRegion(doc, "R2");
        const QString a = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R1", QPointF(0, 0));
        const QString b = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R2", QPointF(1, 1));

        // Missing terminal → reject.
        QVERIFY(!ScenarioMutator::createGlobalLink(
            &doc, "does-not-exist", b, Mode::Ship));
        QVERIFY(doc.globalLinks.isEmpty());

        // Both present → accept, emit globalLinkAdded.
        QSignalSpy spy(&doc, &ScenarioDocument::globalLinkAdded);
        QVERIFY(ScenarioMutator::createGlobalLink(&doc, a, b, Mode::Ship));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(doc.globalLinks.size(), 1);
        QCOMPARE(doc.globalLinks.first().fromTerminalId, a);
        QCOMPARE(doc.globalLinks.first().toTerminalId, b);
        QCOMPARE(doc.globalLinks.first().mode, Mode::Ship);
    }

    void test_remove_global_link_drops_from_document()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R1");
        seedRegion(doc, "R2");
        const QString a = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R1", QPointF(0, 0));
        const QString b = ScenarioMutator::createTerminal(
            &doc, "Sea Port Terminal", "R2", QPointF(1, 1));
        QVERIFY(ScenarioMutator::createGlobalLink(&doc, a, b, Mode::Ship));

        QSignalSpy spy(&doc, &ScenarioDocument::globalLinkRemoved);
        QVERIFY(ScenarioMutator::removeGlobalLink(&doc, a, b, Mode::Ship));
        QVERIFY(doc.globalLinks.isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void test_remove_terminal_drops_from_document()
    {
        ScenarioDocument doc;
        seedRegion(doc, "R");
        const QString id = ScenarioMutator::createTerminal(
            &doc, "Origin", "R", QPointF(0, 0));

        QSignalSpy removedSpy(&doc, &ScenarioDocument::terminalRemoved);
        QVERIFY(ScenarioMutator::removeTerminal(&doc, id));
        QVERIFY(!doc.terminals.contains(id));
        QCOMPARE(removedSpy.count(), 1);
    }
};

QTEST_MAIN(ScenarioMutatorTest)
#include "ScenarioMutatorTest.moc"
