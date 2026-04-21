#include <QPixmap>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/TerminalItem.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

namespace {

/// ConnectionLine requires two graphics endpoints at construction; build
/// a pair of minimal TerminalItems to stand in for them.
struct LineHarness
{
    QPixmap       px{16, 16};
    TerminalItem *a = nullptr;
    TerminalItem *b = nullptr;

    LineHarness()
    {
        px.fill(Qt::red);
        a = new TerminalItem(px, {}, "R", nullptr, "Origin");
        b = new TerminalItem(px, {}, "R", nullptr, "Destination");
    }
    ~LineHarness()
    {
        delete a;
        delete b;
    }
};

/// Seed an existing ScenarioDocument (QObject, non-copyable) with a
/// Connection identified by (from, to, mode, region). Tests construct the
/// doc on the stack and pass it in by pointer.
void seedConnection(ScenarioDocument *doc,
                    const QString &from, const QString &to,
                    Mode mode, const QString &region)
{
    Connection c;
    c.fromTerminalId = from;
    c.toTerminalId   = to;
    c.mode           = mode;
    c.region         = region;
    doc->connections.append(c);
}

void seedGlobalLink(ScenarioDocument *doc,
                    const QString &from, const QString &to, Mode mode)
{
    GlobalLink g;
    g.fromTerminalId = from;
    g.toTerminalId   = to;
    g.mode           = mode;
    doc->globalLinks.append(g);
}

} // namespace

class ConnectionLineViewTest : public QObject
{
    Q_OBJECT
private slots:
    void test_unbound_models_are_null()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        QCOMPARE(line.connectionModel(),
                 static_cast<Connection *>(nullptr));
        QCOMPARE(line.globalLinkModel(),
                 static_cast<GlobalLink *>(nullptr));
        QVERIFY(!line.isConnectionBinding());
        QVERIFY(!line.isGlobalLinkBinding());
    }

    void test_bind_to_connection_resolves_live_pointer()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        ScenarioDocument doc;
        seedConnection(&doc, "A", "B", Mode::Truck, "R");

        line.bindToConnection(&doc, "A", "B", Mode::Truck);
        QVERIFY(line.isConnectionBinding());
        QCOMPARE(line.connectionModel(), &doc.connections.first());
        QVERIFY(line.globalLinkModel() == nullptr);
    }

    void test_connection_lookup_returns_null_after_removal()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        ScenarioDocument doc;
        seedConnection(&doc, "A", "B", Mode::Truck, "R");
        line.bindToConnection(&doc, "A", "B", Mode::Truck);

        doc.connections.clear();

        // The stored key still claims a Connection binding, but the lookup
        // returns nullptr cleanly — no dangling pointer, no UB.
        QVERIFY(line.isConnectionBinding());
        QCOMPARE(line.connectionModel(),
                 static_cast<Connection *>(nullptr));
    }

    void test_binding_global_link_clears_connection_and_vice_versa()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        ScenarioDocument doc;
        seedConnection(&doc, "A", "B", Mode::Truck, "R");
        seedGlobalLink(&doc, "R1/A", "R2/B", Mode::Ship);

        line.bindToConnection(&doc, "A", "B", Mode::Truck);
        QVERIFY(line.isConnectionBinding());
        QCOMPARE(line.connectionModel(), &doc.connections.first());

        // Switch to GlobalLink — connection binding must clear.
        line.bindToGlobalLink(&doc, "R1/A", "R2/B", Mode::Ship);
        QVERIFY(line.isGlobalLinkBinding());
        QVERIFY(!line.isConnectionBinding());
        QCOMPARE(line.globalLinkModel(), &doc.globalLinks.first());
        QCOMPARE(line.connectionModel(),
                 static_cast<Connection *>(nullptr));

        // Switch back — global binding must clear.
        line.bindToConnection(&doc, "A", "B", Mode::Truck);
        QVERIFY(line.isConnectionBinding());
        QVERIFY(!line.isGlobalLinkBinding());
        QCOMPARE(line.connectionModel(), &doc.connections.first());
        QCOMPARE(line.globalLinkModel(),
                 static_cast<GlobalLink *>(nullptr));
    }

    void test_unbind_clears_both_kinds()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        ScenarioDocument doc;
        seedConnection(&doc, "A", "B", Mode::Truck, "R");
        line.bindToConnection(&doc, "A", "B", Mode::Truck);

        line.unbindModel();
        QVERIFY(!line.isConnectionBinding());
        QVERIFY(!line.isGlobalLinkBinding());
        QVERIFY(line.connectionModel() == nullptr);
        QVERIFY(line.globalLinkModel() == nullptr);
        QVERIFY(line.boundFromTerminalId().isEmpty());
        QVERIFY(line.boundToTerminalId().isEmpty());
    }

    void test_global_link_lookup()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Ship, {}, "Global");
        ScenarioDocument doc;
        seedGlobalLink(&doc, "R1/A", "R2/B", Mode::Ship);

        line.bindToGlobalLink(&doc, "R1/A", "R2/B", Mode::Ship);
        QCOMPARE(line.globalLinkModel(), &doc.globalLinks.first());
        QVERIFY(line.connectionModel() == nullptr);
    }
};

QTEST_MAIN(ConnectionLineViewTest)
#include "ConnectionLineViewTest.moc"
