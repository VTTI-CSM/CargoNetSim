// tests/ContainerAllocatorTest.cpp
#include <QCoreApplication>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

#include <containerLib/container.h>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Scenario/ContainerAllocator.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioApplier.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioSerializer.h"

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;

namespace {

// Fabricate a minimal Path via its public constructors (no setters
// exist — verified 2026-04-15).
//
// Path.cpp:256 `getStartTerminal` reads `m_segments.first()->getStart()`
// (not the terminals list); `getEndTerminal` reads
// `m_segments.last()->getEnd()`. ContainerAllocator uses only those
// two methods + `getPathId()`. So a test Path needs:
//   * a valid pathId,
//   * exactly one PathSegment carrying {from, to, mode},
//   * EMPTY terminal list — never read by the allocator.
//
// Path's destructor deletes segments (qDeleteAll in Path.cpp).
// Empty terminal list means no Terminal leak.
Path *makePath(int id,
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
                    QList<PathTerminal>{},               // empty — unused
                    QList<PathSegment *>{ seg });
}

void seed(ScenarioDocument &doc, const QString &id, int count)
{
    TerminalPlacement t;
    t.id     = id;
    t.type   = QStringLiteral("Intermodal Land Terminal");
    t.region = QStringLiteral("USA");
    t.properties[QStringLiteral("initial_container_count")] = count;
    doc.addTerminal(t);

    QList<ContainerCore::Container *> pool;
    for (int i = 0; i < count; ++i)
    {
        auto *c = new ContainerCore::Container();
        c->setContainerID(QString("%1_%2").arg(id).arg(i));
        c->setContainerCurrentLocation(id);
        pool.append(c);
    }
    doc.setOriginContainers(id, std::move(pool));
}

} // namespace

class ContainerAllocatorTest : public QObject
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

    void test_allocate_single_origin_single_destination()
    {
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        doc.addRegion(r);
        TerminalPlacement d;
        d.id = QStringLiteral("D");
        d.type = QStringLiteral("Intermodal Land Terminal");
        d.region = QStringLiteral("USA");
        doc.addTerminal(d);
        seed(doc, QStringLiteral("O"), 5);
        doc.terminals[QStringLiteral("O")]
            .properties[QStringLiteral("destination_terminal")] =
            QStringLiteral("D");

        QList<Path*> paths;
        paths << makePath(42, "O", "D");

        const auto alloc = ContainerAllocator::allocate(doc, paths);
        QCOMPARE(alloc.containersForPath(paths[0]).size(), 5);
        const PathKey expectedKey{ "O", "D", 0 };
        QCOMPARE(alloc.keyForPath(paths[0]), expectedKey);

        qDeleteAll(paths);
    }

    void test_allocate_ignores_paths_without_matching_origin()
    {
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA"); doc.addRegion(r);
        TerminalPlacement d; d.id = "D"; d.type = "Intermodal Land Terminal"; d.region = "USA";
        doc.addTerminal(d);
        seed(doc, "O", 3);
        doc.terminals["O"].properties["destination_terminal"] = QString("D");

        QList<Path*> paths;
        paths << makePath(1, "O", "D");
        paths << makePath(2, "other", "D");  // unrelated origin

        const auto alloc = ContainerAllocator::allocate(doc, paths);
        QCOMPARE(alloc.containersForPath(paths[0]).size(), 3);
        QVERIFY(alloc.containersForPath(paths[1]).isEmpty());
        qDeleteAll(paths);
    }

    void test_allocate_multi_origin_independent_pools()
    {
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);
        TerminalPlacement d; d.id = "D"; d.type = "Intermodal Land Terminal"; d.region = "USA";
        doc.addTerminal(d);
        seed(doc, "O1", 2);
        seed(doc, "O2", 7);
        doc.terminals["O1"].properties["destination_terminal"] = QString("D");
        doc.terminals["O2"].properties["destination_terminal"] = QString("D");

        QList<Path*> paths;
        paths << makePath(10, "O1", "D");
        paths << makePath(20, "O2", "D");

        const auto alloc = ContainerAllocator::allocate(doc, paths);
        QCOMPARE(alloc.containersForPath(paths[0]).size(), 2);
        QCOMPARE(alloc.containersForPath(paths[1]).size(), 7);
        qDeleteAll(paths);
    }

    void test_allocate_splits_by_destination_fractions()
    {
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);
        for (QString id : {"A", "B"}) {
            TerminalPlacement d; d.id = id;
            d.type = "Intermodal Land Terminal"; d.region = "USA";
            doc.addTerminal(d);
        }
        seed(doc, "O", 10);
        QVariantList routes;
        routes << QVariantMap{{"terminal", QString("A")}, {"fraction", 0.6}};
        routes << QVariantMap{{"terminal", QString("B")}, {"fraction", 0.4}};
        doc.terminals["O"].properties["destinations"] = routes;

        QList<Path*> paths;
        paths << makePath(1, "O", "A");
        paths << makePath(2, "O", "B");

        const auto alloc = ContainerAllocator::allocate(doc, paths);
        QCOMPARE(alloc.containersForPath(paths[0]).size(), 6);
        QCOMPARE(alloc.containersForPath(paths[1]).size(), 4);
        qDeleteAll(paths);
    }

    void test_allocate_fraction_rounding_preserves_total()
    {
        // 7 containers across 3 equal fractions: LRM rounding gives
        // 3+2+2 (total 7). No container is dropped or duplicated.
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);
        for (QString id : {"A", "B", "C"}) {
            TerminalPlacement d; d.id = id;
            d.type = "Intermodal Land Terminal"; d.region = "USA";
            doc.addTerminal(d);
        }
        seed(doc, "O", 7);
        QVariantList routes;
        const double third = 1.0 / 3.0;
        routes << QVariantMap{{"terminal", QString("A")}, {"fraction", third}};
        routes << QVariantMap{{"terminal", QString("B")}, {"fraction", third}};
        routes << QVariantMap{{"terminal", QString("C")}, {"fraction", third}};
        doc.terminals["O"].properties["destinations"] = routes;

        QList<Path*> paths;
        paths << makePath(1, "O", "A");
        paths << makePath(2, "O", "B");
        paths << makePath(3, "O", "C");

        const auto alloc = ContainerAllocator::allocate(doc, paths);
        const int total = alloc.containersForPath(paths[0]).size()
                        + alloc.containersForPath(paths[1]).size()
                        + alloc.containersForPath(paths[2]).size();
        QCOMPARE(total, 7);
        qDeleteAll(paths);
    }

    void test_allocate_multi_origin_multi_destination_fixture()
    {
        using namespace CargoNetSim::Backend::Scenario;
        QString err;
        auto doc = ScenarioSerializer::fromYaml(
            QFINDTESTDATA("fixtures/scenario/multi_origin_multi_dest.yml"),
            &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QVERIFY2(ScenarioApplier::apply(*doc, ctl, registry, &err),
                 qPrintable(err));

        QList<Path*> paths;
        paths << makePath(100, "O1", "DA");
        paths << makePath(101, "O1", "DB");
        paths << makePath(200, "O2", "DA");

        const auto alloc = ContainerAllocator::allocate(*doc, paths);
        QCOMPARE(alloc.containersForPath(paths[0]).size(), 6);
        QCOMPARE(alloc.containersForPath(paths[1]).size(), 4);
        QCOMPARE(alloc.containersForPath(paths[2]).size(), 5);

        qDeleteAll(paths);
    }
};

QTEST_MAIN(ContainerAllocatorTest)
#include "ContainerAllocatorTest.moc"
