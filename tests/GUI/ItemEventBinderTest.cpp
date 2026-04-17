#include <QPixmap>
#include <QSignalSpy>
#include <QTest>

#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/ItemEventBinder.h"

class ItemEventBinderTest : public QObject
{
    Q_OBJECT
private slots:
    void test_binder_returns_zero_when_main_window_is_null()
    {
        using namespace CargoNetSim::GUI;
        QPixmap px(16, 16);
        px.fill(Qt::red);
        TerminalItem item(px, {}, "R", nullptr, "Origin");
        // With a null MainWindow, the binder must be a safe no-op
        // and report zero connections established.
        QCOMPARE(
            Scenario::ItemEventBinder::bindTerminalItem(&item, nullptr),
            0);
    }

    void test_binder_returns_zero_when_item_is_null()
    {
        using namespace CargoNetSim::GUI;
        QCOMPARE(
            Scenario::ItemEventBinder::bindTerminalItem(nullptr, nullptr),
            0);
        QCOMPARE(
            Scenario::ItemEventBinder::bindMapPoint(nullptr, nullptr), 0);
        QCOMPARE(
            Scenario::ItemEventBinder::bindConnectionLine(nullptr, nullptr),
            0);
    }
};

QTEST_MAIN(ItemEventBinderTest)
#include "ItemEventBinderTest.moc"
