#include <QCoreApplication>
#include <QTest>

#include <memory>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/PathPreparationService.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/TerminalPlacement.h"

#include <containerLib/container.h>

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;
using Mode = TransportationTypes::TransportationMode;

namespace
{

ScenarioDocument *makeSingleOriginDoc()
{
    auto *doc = new ScenarioDocument();

    RegionSpec r;
    r.name = QStringLiteral("R1");
    doc->addRegion(r);

    TerminalPlacement origin;
    origin.id = QStringLiteral("T1");
    origin.type = QStringLiteral("port");
    origin.region = QStringLiteral("R1");
    origin.properties[QStringLiteral("initial_container_count")] = 50;
    origin.properties[QStringLiteral("destinations")] = QVariantList{
        QVariantMap{{QStringLiteral("terminal"), QStringLiteral("T2")},
                    {QStringLiteral("fraction"), 1.0}}};
    doc->addTerminal(origin);

    QList<ContainerCore::Container *> pool;
    for (int i = 0; i < 50; ++i)
    {
        auto *c = new ContainerCore::Container();
        c->setContainerID(QStringLiteral("T1_%1").arg(i));
        c->setContainerCurrentLocation(QStringLiteral("T1"));
        pool.append(c);
    }
    doc->setOriginContainers(QStringLiteral("T1"), std::move(pool));

    TerminalPlacement destination;
    destination.id = QStringLiteral("T2");
    destination.type = QStringLiteral("port");
    destination.region = QStringLiteral("R1");
    doc->addTerminal(destination);

    return doc;
}

Path *makeEstimatedTrainPath(
    int id, const QString &from, const QString &mid,
    const QString &to)
{
    auto *s1 = new PathSegment(QStringLiteral("s1_%1").arg(from),
                               from, mid, Mode::Train);
    s1->setEstimatedDistanceAndTravelTime(50000.0, 2040.0);

    auto *s2 = new PathSegment(QStringLiteral("s2_%1").arg(to),
                               mid, to, Mode::Train);
    s2->setEstimatedDistanceAndTravelTime(30000.0, 1224.0);

    return new Path(id, 0.0, 0.0, 0.0, {}, {s1, s2});
}

void seedOrigin(ScenarioDocument &doc, const QString &id,
                const QString &destination, int count)
{
    TerminalPlacement origin;
    origin.id = id;
    origin.type = QStringLiteral("Intermodal Land Terminal");
    origin.region = QStringLiteral("USA");
    origin.properties[QStringLiteral("initial_container_count")] = count;
    origin.properties[QStringLiteral("destination_terminal")] =
        destination;
    doc.addTerminal(origin);

    QList<ContainerCore::Container *> pool;
    for (int i = 0; i < count; ++i)
    {
        auto *c = new ContainerCore::Container();
        c->setContainerID(
            QStringLiteral("%1_%2").arg(id).arg(i));
        c->setContainerCurrentLocation(id);
        pool.append(c);
    }
    doc.setOriginContainers(id, std::move(pool));
}

Path *makeDirectEstimatedPath(int id, const QString &from,
                              const QString &to)
{
    auto *segment = new PathSegment(
        QStringLiteral("%1_%2").arg(from, to),
        from, to, Mode::Train);
    segment->setEstimatedDistanceAndTravelTime(10000.0, 600.0);
    return new Path(id, 0.0, 0.0, 0.0, {}, {segment});
}

} // namespace

class PathPreparationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                nullptr, QCoreApplication::instance());
        }
    }

    void test_prepare_paths_populates_container_counts_and_metrics()
    {
        std::unique_ptr<ScenarioDocument> doc(makeSingleOriginDoc());
        auto *config =
            CargoNetSim::CargoNetSimController::getInstance()
                .getConfigController();

        PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {makeEstimatedTrainPath(
                    1, QStringLiteral("T1"),
                    QStringLiteral("MID"),
                    QStringLiteral("T2"))},
                *doc, config,
                /*networks=*/nullptr,
                /*regionData=*/nullptr,
                /*vehicles=*/nullptr);

        QCOMPARE(prepared.size(), 1);
        QCOMPARE(prepared.records().front().path->getEffectiveContainerCount(),
                 50);
        QVERIFY(prepared.records().front().predictedMetrics.valid);

        const QString canonicalPathKey =
            QStringLiteral("odr|T1|T2|0");
        QCOMPARE(prepared.pathIdentities().size(), 1);
        QVERIFY(prepared.predictedMetricsByPathIdentity().contains(
            prepared.pathIdentities().front()));
        QVERIFY(prepared.predictedMetricsByCanonicalPath().contains(
            canonicalPathKey));
        QVERIFY(prepared.pathKeysByCanonicalPath().contains(
            canonicalPathKey));
        QCOMPARE(prepared.pathKeysByCanonicalPath()
                     .value(canonicalPathKey)
                     .originId,
                 QStringLiteral("T1"));
        QCOMPARE(prepared.pathKeysByCanonicalPath()
                     .value(canonicalPathKey)
                     .destinationId,
                 QStringLiteral("T2"));

        const auto preparedPaths = prepared.rawPaths();
        QCOMPARE(preparedPaths.size(), 1);
        QVERIFY(prepared.predictedMetricsByCanonicalPath().contains(
            canonicalPathKey));
    }

    void test_duplicate_local_path_ids_keep_distinct_canonical_identity()
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

        TerminalPlacement db = da;
        db.id = QStringLiteral("DB");
        doc.addTerminal(db);

        seedOrigin(doc, QStringLiteral("O1"), QStringLiteral("DA"), 4);
        seedOrigin(doc, QStringLiteral("O2"), QStringLiteral("DB"), 5);

        auto *config =
            CargoNetSim::CargoNetSimController::getInstance()
                .getConfigController();

        PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {makeDirectEstimatedPath(1, QStringLiteral("O1"),
                                         QStringLiteral("DA")),
                 makeDirectEstimatedPath(1, QStringLiteral("O2"),
                                         QStringLiteral("DB"))},
                doc, config,
                /*networks=*/nullptr,
                /*regionData=*/nullptr,
                /*vehicles=*/nullptr);

        QCOMPARE(prepared.size(), 2);
        QCOMPARE(prepared.pathIdentities().size(), 2);
        QCOMPARE(prepared.predictedMetricsByCanonicalPath().size(), 2);
        QCOMPARE(prepared.pathKeysByCanonicalPath().size(), 2);
        QCOMPARE(prepared.predictedMetricsByPathIdentity().size(), 2);
        QVERIFY(prepared.predictedMetricsByCanonicalPath().contains(
            QStringLiteral("odr|O1|DA|0")));
        QVERIFY(prepared.predictedMetricsByCanonicalPath().contains(
            QStringLiteral("odr|O2|DB|0")));

        const auto preparedPaths = prepared.rawPaths();
        QCOMPARE(preparedPaths.size(), 2);
        QCOMPARE(preparedPaths[0]->getEffectiveContainerCount(), 4);
        QCOMPARE(preparedPaths[1]->getEffectiveContainerCount(), 5);
    }
};

QTEST_MAIN(PathPreparationServiceTest)
#include "PathPreparationServiceTest.moc"
