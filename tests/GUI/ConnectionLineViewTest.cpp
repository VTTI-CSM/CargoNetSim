#include <QPixmap>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
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
    }

    void test_bind_connection_model()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode           = Mode::Truck; c.region    = "R";

        line.setConnectionModel(&c);
        QCOMPARE(line.connectionModel(), &c);
        QVERIFY(line.globalLinkModel() == nullptr);
    }

    void test_binding_global_link_clears_connection_and_vice_versa()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");

        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode = Mode::Truck; c.region = "R";
        line.setConnectionModel(&c);
        QCOMPARE(line.connectionModel(), &c);

        // Switch to a GlobalLink — connection binding must clear.
        GlobalLink g;
        g.fromTerminalId = "R1/A";
        g.toTerminalId   = "R2/B";
        g.mode           = Mode::Ship;
        line.setGlobalLinkModel(&g);
        QCOMPARE(line.globalLinkModel(), &g);
        QCOMPARE(line.connectionModel(),
                 static_cast<Connection *>(nullptr));

        // Switch back — global binding must clear.
        line.setConnectionModel(&c);
        QCOMPARE(line.connectionModel(), &c);
        QCOMPARE(line.globalLinkModel(),
                 static_cast<GlobalLink *>(nullptr));
    }

    void test_unbind_both_via_nullptr()
    {
        LineHarness h;
        ConnectionLine line(h.a, h.b, Mode::Truck, {}, "R");
        Connection c; c.fromTerminalId = "A"; c.toTerminalId = "B";
        line.setConnectionModel(&c);
        line.setConnectionModel(nullptr);
        QVERIFY(line.connectionModel() == nullptr);
    }
};

QTEST_MAIN(ConnectionLineViewTest)
#include "ConnectionLineViewTest.moc"
