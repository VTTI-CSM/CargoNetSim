#include <QTest>

#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/TerminalItem.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class TerminalItemViewTest : public QObject
{
    Q_OBJECT
private slots:
    void test_unbound_item_returns_uuid_from_base()
    {
        QPixmap px(16, 16); px.fill(Qt::red);
        TerminalItem item(px, {}, "R", nullptr, "Origin");
        // Without a placement: getID returns the base-class UUID
        // (QObject identity), and getTerminalId is empty (no domain id).
        QVERIFY(!item.getID().isEmpty());
        QVERIFY(item.getTerminalId().isEmpty());
        QCOMPARE(item.placement(),
                 static_cast<TerminalPlacement *>(nullptr));
    }

    void test_bound_item_exposes_terminal_id()
    {
        TerminalPlacement p;
        p.id     = "T1";
        p.type   = "Sea Port Terminal";
        p.region = "R";
        p.properties["Name"] = "SPT-1";

        QPixmap px(16, 16); px.fill(Qt::red);
        TerminalItem item(px, {}, "R", nullptr, "Sea Port Terminal");
        item.setPlacement(&p);

        // getTerminalId: the domain id. getID: QObject UUID — distinct.
        QCOMPARE(item.getTerminalId(), QString("T1"));
        QVERIFY(item.getID() != QString("T1"));
        QVERIFY(!item.getID().isEmpty());
        // sceneRegistryKey resolves to the domain id when bound.
        QCOMPARE(item.sceneRegistryKey(), QString("T1"));
        QCOMPARE(item.placement(), &p);
        // Snapshot of placement.properties is available through
        // getProperty (read via m_properties cache).
        QCOMPARE(item.getProperty("Name").toString(),
                 QString("SPT-1"));
        QCOMPARE(item.getRegion(), QString("R"));
        QCOMPARE(item.getTerminalType(),
                 QString("Sea Port Terminal"));
    }

    void test_unbind_clears_terminal_id_but_keeps_getid_stable()
    {
        TerminalPlacement p;
        p.id = "T1"; p.type = "Origin"; p.region = "R";

        QPixmap px(16, 16); px.fill(Qt::red);
        TerminalItem item(px, {}, "R", nullptr, "Origin");
        const QString uuid = item.getID();
        item.setPlacement(&p);

        // While bound: getTerminalId returns the placement id; getID stays
        // the same UUID (no override anymore).
        QCOMPARE(item.getTerminalId(), QString("T1"));
        QCOMPARE(item.getID(), uuid);

        item.setPlacement(nullptr);
        QVERIFY(item.placement() == nullptr);
        // After unbind: domain id is empty, but QObject identity (getID)
        // and sceneRegistryKey fallback are stable.
        QVERIFY(item.getTerminalId().isEmpty());
        QCOMPARE(item.getID(), uuid);
        QCOMPARE(item.sceneRegistryKey(), uuid);
    }
};

QTEST_MAIN(TerminalItemViewTest)
#include "TerminalItemViewTest.moc"
