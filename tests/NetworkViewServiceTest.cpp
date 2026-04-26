#include <QCoreApplication>
#include <QTest>

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"

using namespace CargoNetSim;

class NetworkViewServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSimController::instance())
        {
            new CargoNetSimController(
                /*logger=*/nullptr,
                QCoreApplication::instance());
        }

        auto &controller = CargoNetSimController::getInstance();
        auto *rdc = controller.getRegionDataController();
        QVERIFY(rdc != nullptr);

        constexpr auto regionName = "NetworkViewServiceTestRegion";
        constexpr auto networkName = "NetworkViewRail";

        if (!rdc->getRegionData(QString::fromLatin1(regionName)))
            QVERIFY(rdc->addRegion(QString::fromLatin1(regionName)));

        auto *regionData =
            rdc->getRegionData(QString::fromLatin1(regionName));
        QVERIFY(regionData != nullptr);

        if (!regionData->trainNetworkExists(
                QString::fromLatin1(networkName)))
        {
            const QString nodesFile =
                QFINDTESTDATA("fixtures/scenario/rail_nodes.dat");
            const QString linksFile =
                QFINDTESTDATA("fixtures/scenario/rail_links.dat");
            QVERIFY2(!nodesFile.isEmpty(),
                     "nodes fixture must resolve");
            QVERIFY2(!linksFile.isEmpty(),
                     "links fixture must resolve");
            regionData->addTrainNetwork(
                QString::fromLatin1(networkName),
                nodesFile,
                linksFile);
        }

        QVERIFY(rdc->setCurrentRegion(
            QString::fromLatin1(regionName)));
    }

    void test_current_region_and_network_lists_match_region_store()
    {
        Backend::Application::NetworkViewService service(
            CargoNetSimController::instance());

        QCOMPARE(service.currentRegionName(),
                 QStringLiteral("NetworkViewServiceTestRegion"));
        QVERIFY(service.trainNetworkNames(
                    QStringLiteral("NetworkViewServiceTestRegion"))
                    .contains(QStringLiteral("NetworkViewRail")));
        QVERIFY(service.truckNetworkNames(
                    QStringLiteral("NetworkViewServiceTestRegion"))
                    .isEmpty());
        QVERIFY(service.regionOwnsNetwork(
            QStringLiteral("NetworkViewServiceTestRegion"),
            QStringLiteral("NetworkViewRail")));
        QVERIFY(!service.regionOwnsNetwork(
            QStringLiteral("NetworkViewServiceTestRegion"),
            QStringLiteral("DoesNotExist")));
    }

    void test_resolve_node_view_preserves_projection_and_name_contracts()
    {
        Backend::Application::NetworkViewService service(
            CargoNetSimController::instance());

        const auto nodeView = service.resolveNodeView(
            QStringLiteral("NetworkViewServiceTestRegion"),
            QStringLiteral("NetworkViewRail"),
            7);
        QVERIFY(nodeView.has_value());
        QVERIFY(nodeView->isValid());

        auto *regionData =
            CargoNetSimController::getInstance()
                .getRegionDataController()
                ->getRegionData(
                    QStringLiteral("NetworkViewServiceTestRegion"));
        QVERIFY(regionData != nullptr);
        auto *network = regionData->getTrainNetwork(
            QStringLiteral("NetworkViewRail"));
        QVERIFY(network != nullptr);
        auto *node = network->getNodeByID(7);
        QVERIFY(node != nullptr);

        QCOMPARE(nodeView->regionName,
                 QStringLiteral("NetworkViewServiceTestRegion"));
        QCOMPARE(nodeView->networkName,
                 QStringLiteral("NetworkViewRail"));
        QCOMPARE(nodeView->nodeId, 7);
        QCOMPARE(nodeView->kind,
                 CargoNetSim::Backend::NetworkKind::Rail);
        const QPointF expectedProjected(
            node->getX() * node->getXScale(),
            node->getY() * node->getYScale());
        QVERIFY(qAbs(nodeView->projectedScenePoint.x()
                     - expectedProjected.x())
                < 0.01);
        QVERIFY(qAbs(nodeView->projectedScenePoint.y()
                     - expectedProjected.y())
                < 0.01);

        QCOMPARE(service.networkNameOf(nodeView->networkObject),
                 QStringLiteral("NetworkViewRail"));
    }
};

QTEST_MAIN(NetworkViewServiceTest)
#include "NetworkViewServiceTest.moc"
