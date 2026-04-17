#include <QTest>

#include "Backend/Scenario/NodeLinkage.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Scenario/MapPointFactory.h"
#include "GUI/Widgets/GraphicsScene.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

namespace {

/// A MapPoint that carries a linkageModel, matching the state produced
/// by MapPointFactory::fromNodeLinkage. Avoids bringing up a live
/// RegionData for the finder test — we just need the correct
/// Network_ID property + linkageModel.
MapPoint *makeLinkedMapPoint(const QString &networkName, int nodeId,
                             NodeLinkage &backingLinkage)
{
    backingLinkage.networkName = networkName;
    backingLinkage.nodeId      = nodeId;

    QMap<QString, QVariant> props;
    props["Network_ID"] = QString::number(nodeId);
    auto *mp = new MapPoint(QString::number(nodeId),
                            QPointF(0, 0), "R", "circle",
                            nullptr, props);
    mp->setLinkageModel(&backingLinkage);
    return mp;
}

} // namespace

class MapPointFactoryTest : public QObject
{
    Q_OBJECT
private slots:
    void test_returns_null_on_null_linkage()
    {
        GraphicsScene scene;
        QVERIFY(Scenario::MapPointFactory::fromNodeLinkage(
                    nullptr, nullptr, &scene, nullptr)
                == nullptr);
    }

    void test_returns_null_on_null_regiondata()
    {
        NodeLinkage   link;
        GraphicsScene scene;
        QVERIFY(Scenario::MapPointFactory::fromNodeLinkage(
                    &link, nullptr, &scene, nullptr)
                == nullptr);
    }

    void test_returns_null_on_null_scene()
    {
        NodeLinkage link;
        QVERIFY(Scenario::MapPointFactory::fromNodeLinkage(
                    &link, nullptr, nullptr, nullptr)
                == nullptr);
    }

    void test_find_by_network_and_node_matches_linked_map_point()
    {
        GraphicsScene scene;
        NodeLinkage   link1, link2;
        auto *mp1 = makeLinkedMapPoint("rail_net_a", 42, link1);
        auto *mp2 = makeLinkedMapPoint("rail_net_a", 99, link2);
        scene.addItemWithId(mp1, mp1->sceneRegistryKey());
        scene.addItemWithId(mp2, mp2->sceneRegistryKey());

        QCOMPARE(Scenario::MapPointFactory::findByNetworkAndNode(
                     &scene, "rail_net_a", 42),
                 mp1);
        QCOMPARE(Scenario::MapPointFactory::findByNetworkAndNode(
                     &scene, "rail_net_a", 99),
                 mp2);
    }

    void test_find_by_network_and_node_returns_null_on_no_match()
    {
        GraphicsScene scene;
        NodeLinkage   link;
        auto         *mp = makeLinkedMapPoint("rail_net_a", 42, link);
        scene.addItemWithId(mp, mp->sceneRegistryKey());

        // Wrong network.
        QVERIFY(Scenario::MapPointFactory::findByNetworkAndNode(
                    &scene, "truck_net_a", 42)
                == nullptr);
        // Wrong node id.
        QVERIFY(Scenario::MapPointFactory::findByNetworkAndNode(
                    &scene, "rail_net_a", 0)
                == nullptr);
        // Null scene → null.
        QVERIFY(Scenario::MapPointFactory::findByNetworkAndNode(
                    nullptr, "rail_net_a", 42)
                == nullptr);
    }
};

QTEST_MAIN(MapPointFactoryTest)
#include "MapPointFactoryTest.moc"
