#include <QTest>

#include "Backend/Clients/TrainClient/TrainNetwork.h"

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::TrainClient;

class TrainNetworkUnitsTest : public QObject
{
    Q_OBJECT

private slots:
    void test_fixture_is_loaded_with_verified_upstream_units()
    {
        const QString nodesFile =
            QFINDTESTDATA("fixtures/scenario/rail_nodes.dat");
        const QString linksFile =
            QFINDTESTDATA("fixtures/scenario/rail_links.dat");
        QVERIFY2(!nodesFile.isEmpty(), "nodes fixture must resolve");
        QVERIFY2(!linksFile.isEmpty(), "links fixture must resolve");

        NeTrainSimNetwork network;
        network.loadNetwork(nodesFile, linksFile);

        const auto nodes = network.getNodes();
        const auto links = network.getLinks();
        QVERIFY(!nodes.isEmpty());
        QVERIFY(!links.isEmpty());

        const NeTrainSimNode *terminalNode = nullptr;
        for (const auto *node : nodes)
        {
            if (node && node->getUserId() == 7)
            {
                terminalNode = node;
                break;
            }
        }
        QVERIFY(terminalNode != nullptr);
        QCOMPARE(terminalNode->dwellTimeUnits().value(), 1800.0);

        const NeTrainSimLink *firstLink = nullptr;
        for (const auto *link : links)
        {
            if (link && link->getUserId() == 1)
            {
                firstLink = link;
                break;
            }
        }
        QVERIFY(firstLink != nullptr);
        QVERIFY(qAbs(firstLink->lengthUnits().value()
                     - 418639.87) < 0.01);
        QVERIFY(qAbs(firstLink->maxSpeedUnits().value()
                     - 17.88) < 0.001);
    }

    void test_shortest_path_uses_meters_and_seconds()
    {
        const QString nodesFile =
            QFINDTESTDATA("fixtures/scenario/rail_nodes.dat");
        const QString linksFile =
            QFINDTESTDATA("fixtures/scenario/rail_links.dat");
        QVERIFY2(!nodesFile.isEmpty(), "nodes fixture must resolve");
        QVERIFY2(!linksFile.isEmpty(), "links fixture must resolve");

        NeTrainSimNetwork network;
        network.loadNetwork(nodesFile, linksFile);

        const ShortestPathResult result =
            network.findShortestPath(1, 3, "distance");

        QVERIFY(!result.pathNodes.isEmpty());
        QCOMPARE(result.pathNodes.first(), 1);
        QCOMPARE(result.pathNodes.last(), 3);

        const double expectedLengthMeters =
            418639.87 + 194988.43;
        const double expectedTravelTimeSeconds =
            expectedLengthMeters / 17.88;

        QVERIFY(qAbs(result.totalLengthUnits().value()
                     - expectedLengthMeters) < 0.1);
        QVERIFY(qAbs(result.minTravelTimeUnits().value()
                     - expectedTravelTimeSeconds) < 0.1);
    }
};

QTEST_MAIN(TrainNetworkUnitsTest)
#include "TrainNetworkUnitsTest.moc"
