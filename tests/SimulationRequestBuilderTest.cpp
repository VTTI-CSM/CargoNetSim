#include <QCoreApplication>
#include <QDir>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/TrainSystem.h"
#include "Backend/Scenario/PathAllocation.h"
#include "Backend/Scenario/ScenarioApplier.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/RuntimeArtifactIdentity.h"
#include "Backend/Scenario/SimulationRequestBuilder.h"
#include <containerLib/container.h>

class SimulationRequestBuilderTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Tier 1: construct the controller explicitly for this test
        // binary. Parent to QCoreApplication::instance() so Qt cleans
        // it up at the end of the binary's life. Guarded so repeated
        // initTestCase calls in the same binary are safe.
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                /*logger=*/nullptr, QCoreApplication::instance());
        }
    }

    void test_empty_bundle_defaults()
    {
        CargoNetSim::Backend::Scenario::SimulationRequestBundle b;
        QVERIFY(b.shipData.isEmpty());
        QVERIFY(b.trainData.isEmpty());
        QVERIFY(b.truckData.isEmpty());
        QVERIFY(b.trainNetworks.isEmpty());
        QVERIFY(b.truckNetworks.isEmpty());
    }

    void test_train_segment_builds_one_request_per_network()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        const QString fixtureDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario");

        // --- Scenario document ---
        ScenarioDocument doc;
        RegionSpec r;
        r.name = "USA";
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name             = "USA_rail";
        rail.type             = NetworkSpec::Type::Rail;
        rail.files["nodes"]   = fixtureDir + "/rail_nodes.dat";
        rail.files["links"]   = fixtureDir + "/rail_links.dat";
        doc.addNetwork("USA", rail);

        TerminalPlacement o;
        o.id = "T_O"; o.type = "Intermodal Land Terminal"; o.region = "USA";
        o.mode = TerminalPlacement::PositionMode::Scene;
        doc.addTerminal(o);

        TerminalPlacement d;
        d.id = "T_D"; d.type = "Intermodal Land Terminal"; d.region = "USA";
        d.mode = TerminalPlacement::PositionMode::Scene;
        doc.addTerminal(d);

        NodeLinkage la;
        la.terminalId = "T_O"; la.networkName = "USA_rail"; la.nodeId = 1;
        doc.addLinkage(la);

        NodeLinkage lb;
        lb.terminalId = "T_D"; lb.networkName = "USA_rail"; lb.nodeId = 2;
        doc.addLinkage(lb);

        doc.simulation.rail.containers = 10;
        doc.fleet.trainsFiles = { fixtureDir + "/fleet_trains.json" };

        // --- Materialize backend state ---
        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        // --- Path with one Train segment ---
        auto *segment = new PathSegment(
            "seg0", "T_O", "T_D",
            TransportationTypes::TransportationMode::Train);
        QList<PathTerminal>  emptyTerminals;
        QList<PathSegment *> segments{segment};
        auto *path = new Path(1, 0.0, 0.0, 0.0,
                              emptyTerminals, segments);

        // --- 5 origin containers ---
        QList<ContainerCore::Container *> originContainers;
        for (int i = 0; i < 5; ++i)
        {
            auto *c = new ContainerCore::Container();
            c->setContainerID(QString("C%1").arg(i));
            originContainers.append(c);
        }

        // --- Run builder ---
        SimulationRequestBuilder builder(
            doc, registry,
            ctl.getConfigController(),
            ctl.getRegionDataController(),
            ctl.getVehicleController());
        SimulationRequestBundle bundle;
        QVERIFY2(builder.buildForPath(path, originContainers,
                                      bundle, &err),
                 qPrintable(err));

        // --- Assertions ---
        QVERIFY(bundle.trainData.contains("USA_rail"));
        QCOMPARE(bundle.trainData["USA_rail"].size(), 1);
        const auto &td = bundle.trainData["USA_rail"].first();
        QCOMPARE(td.containers.size(), 5);
        QVERIFY(td.train != nullptr);
        RuntimeArtifactIdentity trainId;
        QVERIFY(RuntimeArtifacts::decode(td.train->getUserId(), trainId));
        QCOMPARE(trainId.artifactType, QStringLiteral("train"));
        QCOMPARE(trainId.segmentIndex, 0);
        QCOMPARE(trainId.artifactIndex, 0);

        // --- Cleanup ---
        qDeleteAll(originContainers);
        delete path;  // owns segments
        for (auto &list : bundle.trainData)
            for (auto &item : list)
            {
                delete item.train;
                qDeleteAll(item.containers);
            }
    }

    void test_truck_segment_builds_trip_per_truck()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        const QString fixtureDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario");

        // --- Scenario document ---
        ScenarioDocument doc;
        RegionSpec r;
        r.name = "USA";
        doc.addRegion(r);

        NetworkSpec truck;
        truck.name             = "USA_truck";
        truck.type             = NetworkSpec::Type::Truck;
        truck.files["config"]  = fixtureDir + "/truck_config.dat";
        doc.addNetwork("USA", truck);

        TerminalPlacement o;
        o.id = "T_O"; o.type = "Truck Parking"; o.region = "USA";
        o.mode = TerminalPlacement::PositionMode::Scene;
        o.properties["Name"] = QString("Origin");
        doc.addTerminal(o);

        TerminalPlacement d;
        d.id = "T_D"; d.type = "Truck Parking"; d.region = "USA";
        d.mode = TerminalPlacement::PositionMode::Scene;
        d.properties["Name"] = QString("Destination");
        doc.addTerminal(d);

        NodeLinkage la;
        la.terminalId = "T_O"; la.networkName = "USA_truck"; la.nodeId = 1;
        doc.addLinkage(la);

        NodeLinkage lb;
        lb.terminalId = "T_D"; lb.networkName = "USA_truck"; lb.nodeId = 2;
        doc.addLinkage(lb);

        doc.simulation.truck.containers = 2;  // 5 containers → 3 trucks

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        auto *segment = new PathSegment(
            "seg0", "T_O", "T_D",
            TransportationTypes::TransportationMode::Truck);
        QList<PathTerminal>  emptyTerminals;
        QList<PathSegment *> segments{segment};
        auto *path = new Path(1, 0.0, 0.0, 0.0,
                              emptyTerminals, segments);

        QList<ContainerCore::Container *> originContainers;
        for (int i = 0; i < 5; ++i)
        {
            auto *c = new ContainerCore::Container();
            c->setContainerID(QString("C%1").arg(i));
            originContainers.append(c);
        }

        SimulationRequestBuilder builder(
            doc, registry,
            ctl.getConfigController(),
            ctl.getRegionDataController(),
            ctl.getVehicleController());
        SimulationRequestBundle bundle;
        QVERIFY2(builder.buildForPath(path, originContainers,
                                      bundle, &err),
                 qPrintable(err));

        QVERIFY(bundle.truckData.contains("USA_truck"));
        QCOMPARE(bundle.truckData["USA_truck"].size(), 3);
        const auto &trips = bundle.truckData["USA_truck"];
        RuntimeArtifactIdentity trip0;
        RuntimeArtifactIdentity trip1;
        RuntimeArtifactIdentity trip2;
        QVERIFY(RuntimeArtifacts::decode(trips[0].tripId, trip0));
        QVERIFY(RuntimeArtifacts::decode(trips[1].tripId, trip1));
        QVERIFY(RuntimeArtifacts::decode(trips[2].tripId, trip2));
        QCOMPARE(trip0.artifactIndex, 0);
        QCOMPARE(trip1.artifactIndex, 1);
        QCOMPARE(trip2.artifactIndex, 2);
        QCOMPARE(trips[0].originNode,      1);
        QCOMPARE(trips[0].destinationNode, 2);
        // 5 containers across 3 trucks: 2 + 2 + 1
        QCOMPARE(trips[0].containers.size(), 2);
        QCOMPARE(trips[1].containers.size(), 2);
        QCOMPARE(trips[2].containers.size(), 1);

        qDeleteAll(originContainers);
        delete path;
        for (auto &list : bundle.truckData)
            for (auto &item : list)
                qDeleteAll(item.containers);
    }

    void test_ship_segment_builds_per_region_network()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        const QString fixtureDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario");

        // --- Scenario document ---
        ScenarioDocument doc;
        RegionSpec r;
        r.name                   = "USA";
        r.localOrigin.latitude   = 40.0;
        r.localOrigin.longitude  = -74.0;
        r.globalPosition.latitude  = 40.0;
        r.globalPosition.longitude = -74.0;
        doc.addRegion(r);

        TerminalPlacement o;
        o.id = "SP_A"; o.type = "Sea Port Terminal"; o.region = "USA";
        o.mode = TerminalPlacement::PositionMode::LatLon;
        o.latLon.latitude  = 40.7;
        o.latLon.longitude = -74.0;
        doc.addTerminal(o);

        TerminalPlacement d;
        d.id = "SP_B"; d.type = "Sea Port Terminal"; d.region = "USA";
        d.mode = TerminalPlacement::PositionMode::LatLon;
        d.latLon.latitude  = 41.0;
        d.latLon.longitude = -73.5;
        doc.addTerminal(d);

        doc.simulation.ship.containers = 10;  // 5 containers → 1 ship
        doc.fleet.shipsFiles = { fixtureDir + "/fleet_ships.json" };

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        auto *segment = new PathSegment(
            "seg0", "SP_A", "SP_B",
            TransportationTypes::TransportationMode::Ship);
        QList<PathTerminal>  emptyTerminals;
        QList<PathSegment *> segments{segment};
        auto *path = new Path(1, 0.0, 0.0, 0.0,
                              emptyTerminals, segments);

        QList<ContainerCore::Container *> originContainers;
        for (int i = 0; i < 5; ++i)
        {
            auto *c = new ContainerCore::Container();
            c->setContainerID(QString("C%1").arg(i));
            originContainers.append(c);
        }

        SimulationRequestBuilder builder(
            doc, registry,
            ctl.getConfigController(),
            ctl.getRegionDataController(),
            ctl.getVehicleController());
        SimulationRequestBundle bundle;
        QVERIFY2(builder.buildForPath(path, originContainers,
                                      bundle, &err),
                 qPrintable(err));

        QVERIFY(bundle.shipData.contains("USA"));
        QCOMPARE(bundle.shipData["USA"].size(), 1);
        const auto &sd = bundle.shipData["USA"].first();
        QVERIFY(sd.ship != nullptr);
        RuntimeArtifactIdentity shipId;
        QVERIFY(RuntimeArtifacts::decode(sd.ship->getUserId(), shipId));
        QCOMPARE(shipId.artifactType, QStringLiteral("ship"));
        QCOMPARE(shipId.segmentIndex, 0);
        QCOMPARE(shipId.artifactIndex, 0);
        QCOMPARE(sd.destinationTerminal,        QString("SP_B"));
        QCOMPARE(sd.containers.size(),          5);

        // Ship stores coordinates as QVector<QVector<float>> [[x=lon, y=lat], ...].
        const auto pathCoords = sd.ship->getPathCoordinates();
        QCOMPARE(pathCoords.size(), 2);
        QCOMPARE(pathCoords[0].size(), 2);
        QCOMPARE(pathCoords[1].size(), 2);
        QVERIFY(qFuzzyCompare(pathCoords[0][0], -74.0f));  // SP_A lon
        QVERIFY(qFuzzyCompare(pathCoords[0][1],  40.7f));  // SP_A lat
        QVERIFY(qFuzzyCompare(pathCoords[1][0], -73.5f));  // SP_B lon
        QVERIFY(qFuzzyCompare(pathCoords[1][1],  41.0f));  // SP_B lat

        qDeleteAll(originContainers);
        delete path;
        for (auto &list : bundle.shipData)
            for (auto &item : list)
            {
                delete item.ship;
                qDeleteAll(item.containers);
            }
    }

    void test_build_merges_multiple_paths_on_same_network()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        const QString fixtureDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario");

        // --- One region, one rail network, four terminals (two pairs) ---
        ScenarioDocument doc;
        RegionSpec r;
        r.name = "USA";
        doc.addRegion(r);

        NetworkSpec rail;
        rail.name             = "USA_rail";
        rail.type             = NetworkSpec::Type::Rail;
        rail.files["nodes"]   = fixtureDir + "/rail_nodes.dat";
        rail.files["links"]   = fixtureDir + "/rail_links.dat";
        doc.addNetwork("USA", rail);

        auto addTerm = [&](const QString &id, int nodeId)
        {
            TerminalPlacement t;
            t.id = id; t.type = "Intermodal Land Terminal"; t.region = "USA";
            t.mode = TerminalPlacement::PositionMode::Scene;
            doc.addTerminal(t);

            NodeLinkage l;
            l.terminalId = id; l.networkName = "USA_rail"; l.nodeId = nodeId;
            doc.addLinkage(l);
        };
        addTerm("T_A", 1);
        addTerm("T_B", 2);
        addTerm("T_C", 3);
        addTerm("T_D", 4);

        doc.simulation.rail.containers = 10;  // 5 containers → 1 train per path
        doc.fleet.trainsFiles = { fixtureDir + "/fleet_trains.json" };

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        // --- Two paths: T_A→T_B (id=1) and T_C→T_D (id=2) ---
        auto makeRailPath =
            [](int pathId, const QString &s, const QString &e) -> Path *
        {
            auto *seg = new PathSegment(
                "seg0", s, e,
                TransportationTypes::TransportationMode::Train);
            QList<PathTerminal>  termsNone;
            QList<PathSegment *> segs{seg};
            return new Path(pathId, 0.0, 0.0, 0.0, termsNone, segs);
        };
        auto *p1 = makeRailPath(1, "T_A", "T_B");
        auto *p2 = makeRailPath(2, "T_C", "T_D");

        QList<ContainerCore::Container *> originContainers;
        for (int i = 0; i < 5; ++i)
        {
            auto *c = new ContainerCore::Container();
            c->setContainerID(QString("C%1").arg(i));
            originContainers.append(c);
        }

        SimulationRequestBuilder builder(
            doc, registry,
            ctl.getConfigController(),
            ctl.getRegionDataController(),
            ctl.getVehicleController());

        SimulationRequestBundle bundle;
        // Plan 10: build's signature now takes PathAllocation. Preserve
        // the test's pre-Plan-10 semantics (each path gets the full
        // 5-container pool) by assigning the same list to both path ids.
        CargoNetSim::Backend::Scenario::PathAllocation alloc;
        alloc.byCanonicalPath.insert(p1->canonicalPathKey(), originContainers);
        alloc.byCanonicalPath.insert(p2->canonicalPathKey(), originContainers);

        QVERIFY2(builder.build({p1, p2}, alloc, bundle, &err),
                 qPrintable(err));

        // --- Both paths contribute to USA_rail; merge preserves all entries ---
        QVERIFY(bundle.trainData.contains("USA_rail"));
        QCOMPARE(bundle.trainData["USA_rail"].size(), 2);

        QStringList userIds;
        for (const auto &td : bundle.trainData["USA_rail"])
            userIds << td.train->getUserId();
        QCOMPARE(userIds.size(), 2);
        QVERIFY(userIds[0] != userIds[1]);

        for (const auto &td : bundle.trainData["USA_rail"])
            QCOMPARE(td.containers.size(), 5);

        qDeleteAll(originContainers);
        delete p1;
        delete p2;
        for (auto &list : bundle.trainData)
            for (auto &item : list)
            {
                delete item.train;
                qDeleteAll(item.containers);
            }
    }
};

QTEST_MAIN(SimulationRequestBuilderTest)
#include "SimulationRequestBuilderTest.moc"
