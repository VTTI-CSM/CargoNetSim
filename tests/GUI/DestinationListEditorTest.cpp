// tests/GUI/DestinationListEditorTest.cpp
#include <QApplication>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>

#include "GUI/Widgets/DestinationListEditor.h"

using namespace CargoNetSim::GUI;
using namespace CargoNetSim::Backend::Scenario;

class DestinationListEditorTest : public QObject
{
    Q_OBJECT
private slots:
    void test_set_routes_populates_table()
    {
        DestinationListEditor ed;

        ed.setRoutes({ { "A", 0.6 }, { "B", 0.4 } });
        const auto rt = ed.routes();
        QCOMPARE(rt.size(), 2);
        QCOMPARE(rt[0].terminal, QString("A"));
        QCOMPARE(rt[0].fraction, 0.6);
        QCOMPARE(rt[1].terminal, QString("B"));
        QCOMPARE(rt[1].fraction, 0.4);
    }

    void test_isValid_true_on_well_formed_list()
    {
        DestinationListEditor ed;

        ed.setRoutes({ { "A", 0.7 }, { "B", 0.3 } });
        QVERIFY(ed.isValid());
    }

    void test_isValid_false_when_sum_off()
    {
        DestinationListEditor ed;

        ed.setRoutes({ { "A", 0.7 }, { "B", 0.2 } });
        QVERIFY(!ed.isValid());
    }

    void test_isValid_false_on_duplicate_terminal()
    {
        DestinationListEditor ed;

        ed.setRoutes({ { "A", 0.5 }, { "A", 0.5 } });
        QVERIFY(!ed.isValid());
    }

    void test_isValid_false_on_empty()
    {
        DestinationListEditor ed;

        QVERIFY(!ed.isValid());
    }

    void test_changed_signal_fires_on_programmatic_setRoutes()
    {
        DestinationListEditor ed;

        QSignalSpy spy(&ed, &DestinationListEditor::changed);
        ed.setRoutes({ { "A", 1.0 } });
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(DestinationListEditorTest)
#include "DestinationListEditorTest.moc"
