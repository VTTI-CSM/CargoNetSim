#include <QPixmap>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/ConnectionLineFactory.h"
#include "GUI/Widgets/GraphicsScene.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

namespace {

/// Build a TerminalItem bound to a placement so its `getID()` returns
/// the placement id (not the base-class auto-UUID). Caller owns both.
struct BoundTerminal
{
    TerminalPlacement placement;
    TerminalItem     *item = nullptr;

    BoundTerminal(const QString &id, const QString &region)
    {
        placement.id     = id;
        placement.type   = "Origin";
        placement.region = region;
        placement.mode   = TerminalPlacement::PositionMode::LatLon;
        QPixmap px(16, 16); px.fill(Qt::red);
        item = new TerminalItem(px, {}, region, nullptr, "Origin");
        item->setPlacement(&placement);
    }
    ~BoundTerminal() { delete item; }
};

} // namespace

class ConnectionLineFactoryTest : public QObject
{
    Q_OBJECT
private slots:
    void test_from_connection_returns_null_on_null_inputs()
    {
        GraphicsScene scene;
        ScenarioDocument doc;
        QVERIFY(Scenario::ConnectionLineFactory::fromConnection(
                    &doc, nullptr, &scene, nullptr)
                == nullptr);
        Connection c;
        QVERIFY(Scenario::ConnectionLineFactory::fromConnection(
                    &doc, &c, nullptr, nullptr)
                == nullptr);
        QVERIFY(Scenario::ConnectionLineFactory::fromConnection(
                    nullptr, &c, &scene, nullptr)
                == nullptr);
    }

    void test_from_connection_returns_null_when_endpoints_missing()
    {
        GraphicsScene scene;
        ScenarioDocument doc;
        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B"; c.mode = Mode::Truck;
        QVERIFY(Scenario::ConnectionLineFactory::fromConnection(
                    &doc, &c, &scene, nullptr)
                == nullptr);
    }

    void test_from_connection_creates_line_between_endpoints()
    {
        GraphicsScene scene;
        BoundTerminal a("A", "R"), b("B", "R");
        // Register under sceneRegistryKey() — matches production code and
        // resolves to the bound placement id ("A"/"B") via domainKey().
        scene.addItemWithId(a.item, a.item->sceneRegistryKey());
        scene.addItemWithId(b.item, b.item->sceneRegistryKey());

        // Put the connection inside the doc so the line's stable-key
        // binding can resolve it back via connectionModel().
        ScenarioDocument doc;
        Connection c;
        c.fromTerminalId = "A";
        c.toTerminalId   = "B";
        c.mode           = Mode::Truck;
        c.region         = "R";
        doc.connections.append(c);

        auto *line = Scenario::ConnectionLineFactory::fromConnection(
            &doc, &doc.connections.first(), &scene, nullptr);

        QVERIFY(line != nullptr);
        QVERIFY(line->isConnectionBinding());
        QCOMPARE(line->connectionModel(), &doc.connections.first());
        QCOMPARE(line->globalLinkModel(),
                 static_cast<GlobalLink *>(nullptr));
        QVERIFY(scene.items().contains(line));
    }

    void test_from_global_link_returns_null_without_matching_global_terminal()
    {
        GraphicsScene scene;
        ScenarioDocument doc;
        GlobalLink g;
        g.fromTerminalId = "A"; g.toTerminalId = "B"; g.mode = Mode::Ship;
        QVERIFY(Scenario::ConnectionLineFactory::fromGlobalLink(
                    &doc, &g, &scene, nullptr)
                == nullptr);
    }

    void test_from_global_link_finds_endpoints_via_linked_terminal_id()
    {
        GraphicsScene scene;
        BoundTerminal a("A", "R1"), b("B", "R2");

        QPixmap px(16, 16); px.fill(Qt::blue);
        auto *ga = new GlobalTerminalItem(px, a.item);
        auto *gb = new GlobalTerminalItem(px, b.item);
        // GlobalTerminalItem::sceneRegistryKey() delegates through
        // linkedTerminalItem->getTerminalId() → placement.id, so the
        // registry key IS the terminal id — "A" / "B" here.
        scene.addItemWithId(ga, ga->sceneRegistryKey());
        scene.addItemWithId(gb, gb->sceneRegistryKey());

        ScenarioDocument doc;
        GlobalLink g;
        g.fromTerminalId = "A";  // same string as the registry key
        g.toTerminalId   = "B";
        g.mode           = Mode::Ship;
        doc.globalLinks.append(g);

        auto *line = Scenario::ConnectionLineFactory::fromGlobalLink(
            &doc, &doc.globalLinks.first(), &scene, nullptr);

        QVERIFY(line != nullptr);
        QVERIFY(line->isGlobalLinkBinding());
        QCOMPARE(line->globalLinkModel(), &doc.globalLinks.first());
        QCOMPARE(line->connectionModel(),
                 static_cast<Connection *>(nullptr));
    }
};

QTEST_MAIN(ConnectionLineFactoryTest)
#include "ConnectionLineFactoryTest.moc"
