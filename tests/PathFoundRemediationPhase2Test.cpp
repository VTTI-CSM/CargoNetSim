#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/ContainerAllocator.h"
#include "Backend/Scenario/PathAllocation.h"
#include "Backend/Scenario/ScenarioApplier.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/RuntimeArtifactIdentity.h"
#include "Backend/Scenario/SimulationRequestBuilder.h"
#include <containerLib/container.h>

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;

namespace {

Path *makePath(
    int id,
    const QString &from,
    const QString &to,
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Train)
{
    auto *seg = new PathSegment(
        QStringLiteral("%1_%2_%3").arg(from, to).arg(int(mode)),
        from, to, mode);
    return new Path(id,
                    /*totalCost=*/0.0,
                    /*edgeCost=*/0.0,
                    /*termCost=*/0.0,
                    QList<PathTerminal>{},
                    QList<PathSegment *>{seg});
}

void seedOrigin(ScenarioDocument &doc, const QString &id, int count)
{
    TerminalPlacement t;
    t.id = id;
    t.type = QStringLiteral("Intermodal Land Terminal");
    t.region = QStringLiteral("USA");
    t.properties[QStringLiteral("initial_container_count")] = count;
    doc.addTerminal(t);

    QList<ContainerCore::Container *> pool;
    for (int i = 0; i < count; ++i)
    {
        auto *c = new ContainerCore::Container();
        c->setContainerID(QStringLiteral("%1_%2").arg(id).arg(i));
        c->setContainerCurrentLocation(id);
        pool.append(c);
    }
    doc.setOriginContainers(id, std::move(pool));
}

QJsonObject loadFixture()
{
    QFile f(QFINDTESTDATA(
        "fixtures/pathfound/multi_origin_multi_destination_pathfound.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

class PathFoundRemediationPhase2Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                /*logger=*/nullptr, QCoreApplication::instance());
        }
    }

    void test_multi_origin_fixture_requires_identity_beyond_local_path_id()
    {
        const QJsonObject fixture = loadFixture();
        QVERIFY(!fixture.isEmpty());

        const QJsonArray responses =
            fixture.value(QStringLiteral("responses")).toArray();
        QVERIFY(!responses.isEmpty());

        QSet<int> localIds;
        int totalPaths = 0;
        for (const QJsonValue &responseValue : responses)
        {
            const QJsonObject result =
                responseValue.toObject().value(QStringLiteral("result")).toObject();
            const QJsonArray paths =
                result.value(QStringLiteral("paths")).toArray();
            totalPaths += paths.size();
            for (const QJsonValue &pathValue : paths)
            {
                localIds.insert(
                    pathValue.toObject().value(QStringLiteral("path_id")).toInt());
            }
        }

        QVERIFY2(localIds.size() < totalPaths,
                 "historical fixture should prove that local path_id values are reused across origin/destination pairs");
    }

    void test_container_allocator_collides_when_two_pairs_share_local_path_id()
    {
        ScenarioDocument doc;
        RegionSpec usa;
        usa.name = QStringLiteral("USA");
        doc.addRegion(usa);

        TerminalPlacement da;
        da.id = QStringLiteral("DA");
        da.type = QStringLiteral("Intermodal Land Terminal");
        da.region = QStringLiteral("USA");
        doc.addTerminal(da);

        TerminalPlacement db;
        db.id = QStringLiteral("DB");
        db.type = QStringLiteral("Intermodal Land Terminal");
        db.region = QStringLiteral("USA");
        doc.addTerminal(db);

        seedOrigin(doc, QStringLiteral("O1"), 4);
        seedOrigin(doc, QStringLiteral("O2"), 5);

        doc.terminals[QStringLiteral("O1")]
            .properties[QStringLiteral("destination_terminal")] =
            QStringLiteral("DA");
        doc.terminals[QStringLiteral("O2")]
            .properties[QStringLiteral("destination_terminal")] =
            QStringLiteral("DB");

        QList<Path *> paths;
        paths << makePath(1, QStringLiteral("O1"), QStringLiteral("DA"))
              << makePath(1, QStringLiteral("O2"), QStringLiteral("DB"));

        const auto alloc = ContainerAllocator::allocate(doc, paths);

        QCOMPARE(alloc.keyByCanonicalPath.size(), 2);
        QCOMPARE(alloc.byCanonicalPath.size(), 2);

        qDeleteAll(paths);
    }

    void test_simulation_request_artifact_ids_must_not_collide_for_duplicate_local_path_ids()
    {
        const QString fixtureDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath(QStringLiteral("fixtures/scenario"));

        ScenarioDocument doc;
        RegionSpec usa;
        usa.name = QStringLiteral("USA");
        doc.addRegion(usa);

        NetworkSpec rail;
        rail.name = QStringLiteral("USA_rail");
        rail.type = NetworkSpec::Type::Rail;
        rail.files[QStringLiteral("nodes")] =
            fixtureDir + QStringLiteral("/rail_nodes.dat");
        rail.files[QStringLiteral("links")] =
            fixtureDir + QStringLiteral("/rail_links.dat");
        doc.addNetwork(QStringLiteral("USA"), rail);

        TerminalPlacement o1;
        o1.id = QStringLiteral("O1");
        o1.type = QStringLiteral("Intermodal Land Terminal");
        o1.region = QStringLiteral("USA");
        o1.mode = TerminalPlacement::PositionMode::Scene;
        doc.addTerminal(o1);

        TerminalPlacement o2 = o1;
        o2.id = QStringLiteral("O2");
        doc.addTerminal(o2);

        TerminalPlacement d;
        d.id = QStringLiteral("D");
        d.type = QStringLiteral("Intermodal Land Terminal");
        d.region = QStringLiteral("USA");
        d.mode = TerminalPlacement::PositionMode::Scene;
        doc.addTerminal(d);

        NodeLinkage o1Link;
        o1Link.terminalId = QStringLiteral("O1");
        o1Link.networkName = QStringLiteral("USA_rail");
        o1Link.nodeId = 1;
        doc.addLinkage(o1Link);

        NodeLinkage o2Link = o1Link;
        o2Link.terminalId = QStringLiteral("O2");
        o2Link.nodeId = 1;
        doc.addLinkage(o2Link);

        NodeLinkage dLink = o1Link;
        dLink.terminalId = QStringLiteral("D");
        dLink.nodeId = 2;
        doc.addLinkage(dLink);

        doc.simulation.rail.containers = 10;
        doc.fleet.trainsFiles = {fixtureDir + QStringLiteral("/fleet_trains.json")};

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        QList<ContainerCore::Container *> firstPool;
        QList<ContainerCore::Container *> secondPool;
        for (int i = 0; i < 4; ++i)
        {
            auto *c1 = new ContainerCore::Container();
            c1->setContainerID(QStringLiteral("O1_C%1").arg(i));
            firstPool.append(c1);

            auto *c2 = new ContainerCore::Container();
            c2->setContainerID(QStringLiteral("O2_C%1").arg(i));
            secondPool.append(c2);
        }

        std::unique_ptr<Path> first(
            makePath(1, QStringLiteral("O1"), QStringLiteral("D")));
        std::unique_ptr<Path> second(
            makePath(1, QStringLiteral("O2"), QStringLiteral("D")));

        SimulationRequestBuilder builder(
            doc, registry, ctl.getConfigController(),
            ctl.getRegionDataController(), ctl.getVehicleController());
        SimulationRequestBundle bundle;
        QVERIFY2(builder.buildForPath(first.get(), firstPool, bundle, &err),
                 qPrintable(err));
        QVERIFY2(builder.buildForPath(second.get(), secondPool, bundle, &err),
                 qPrintable(err));

        QVERIFY(bundle.trainData.contains(QStringLiteral("USA_rail")));
        const auto trains = bundle.trainData.value(QStringLiteral("USA_rail"));
        QCOMPARE(trains.size(), 2);
        QVERIFY(trains[0].train != nullptr);
        QVERIFY(trains[1].train != nullptr);
        QVERIFY2(trains[0].train->getUserId() != trains[1].train->getUserId(),
                 "duplicate runtime artifact IDs still collide when two paths share a local path_id");
        RuntimeArtifactIdentity firstId;
        RuntimeArtifactIdentity secondId;
        QVERIFY(RuntimeArtifacts::decode(trains[0].train->getUserId(), firstId));
        QVERIFY(RuntimeArtifacts::decode(trains[1].train->getUserId(), secondId));
        QVERIFY(firstId.pathKey != secondId.pathKey);

        qDeleteAll(firstPool);
        qDeleteAll(secondPool);
        for (const auto &trainData : trains)
        {
            delete trainData.train;
            qDeleteAll(trainData.containers);
        }
    }

    void test_save_reopen_round_trip_must_preserve_comparison_snapshots()
    {
        QString err;
        auto doc = ScenarioSerializer::fromYaml(
            QFINDTESTDATA("fixtures/scenario/minimal.yml"), &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        QJsonObject serialized = ScenarioSerializer::toJson(*doc);
        const QJsonArray comparisonSnapshots{
            QJsonObject{
                {QStringLiteral("schema_version"), 1},
                {QStringLiteral("path_uid"), QStringLiteral("uid-O1-DA-r0")}
            }
        };
        serialized[QStringLiteral("comparison_snapshots")] = comparisonSnapshots;

        auto reopened = ScenarioSerializer::fromJson(serialized);
        QVERIFY(reopened != nullptr);

        const QJsonObject roundTripped =
            ScenarioSerializer::toJson(*reopened);
        QVERIFY(roundTripped.contains(QStringLiteral("comparison_snapshots")));
        QCOMPARE(roundTripped.value(QStringLiteral("comparison_snapshots")).toArray(),
                 comparisonSnapshots);
    }
};

QTEST_MAIN(PathFoundRemediationPhase2Test)
#include "PathFoundRemediationPhase2Test.moc"
