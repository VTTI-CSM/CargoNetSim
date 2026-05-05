#include <QTest>

#include "Backend/Scenario/NodeLinkage.h"
#include "GUI/Items/MapPoint.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class MapPointViewTest : public QObject
{
    Q_OBJECT
private slots:
    void test_unbound_linkage_model_is_null()
    {
        MapPoint mp("42", QPointF(0, 0), "R");
        QCOMPARE(mp.linkageModel(),
                 static_cast<NodeLinkage *>(nullptr));
    }

    void test_bind_and_unbind_linkage_model()
    {
        NodeLinkage link;
        link.terminalId  = "T1";
        link.networkName = "rail_net";
        link.nodeId      = 42;

        MapPoint mp("42", QPointF(0, 0), "R");
        mp.setLinkageModel(&link);
        QCOMPARE(mp.linkageModel(), &link);

        // Basic field accessors through the returned pointer.
        QCOMPARE(mp.linkageModel()->networkName, QString("rail_net"));
        QCOMPARE(mp.linkageModel()->nodeId, 42);

        mp.setLinkageModel(nullptr);
        QVERIFY(mp.linkageModel() == nullptr);
    }
};

QTEST_MAIN(MapPointViewTest)
#include "MapPointViewTest.moc"
