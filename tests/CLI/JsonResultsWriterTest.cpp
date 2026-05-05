#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include "Backend/Scenario/PathKey.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/PathSimulationResult.h"
#include "CLI/Output/JsonResultsWriter.h"

// Contract lock-tests for JsonResultsWriter (Plan 5 Task 6).
//
// The emitted shape is a published contract (consumers parse these
// files from shell scripts and CI). Fields or layout must not drift
// silently — each test below asserts one slice of that contract.

class JsonResultsWriterTest : public QObject
{
    Q_OBJECT

private:
    using PSR = CargoNetSim::Backend::Scenario::PathSimulationResult;

    /// Read @p path back as a parsed QJsonObject. Asserts parse success
    /// so each test's body stays focused on the shape it actually
    /// wants to verify.
    QJsonObject readBack(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject())
            return {};
        return doc.object();
    }

private slots:
    void test_writes_one_object_per_path_with_all_four_fields()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        PSR r;
        r.pathId        = 1;
        r.pathUid       = QStringLiteral("uid-1");
        r.canonicalPathKey = QStringLiteral("uid-1");
        r.totalCost     = 123.45;
        r.edgeCosts     = 100.0;
        r.terminalCosts = 23.45;

        Cli::JsonResultsWriter w;
        QString                err;
        QVERIFY2(w.write(out, {r}, &err), qPrintable(err));

        const QJsonObject root  = readBack(out);
        const QJsonArray  paths = root.value(QStringLiteral("paths")).toArray();
        QCOMPARE(paths.size(), 1);
        const QJsonObject p = paths.first().toObject();
        QCOMPARE(p.value(QStringLiteral("path_id")).toInt(),          1);
        QCOMPARE(p.value(QStringLiteral("path_uid")).toString(), QStringLiteral("uid-1"));
        QCOMPARE(p.value(QStringLiteral("total_cost")).toDouble(),    123.45);
        QCOMPARE(p.value(QStringLiteral("edge_costs")).toDouble(),    100.0);
        QCOMPARE(p.value(QStringLiteral("terminal_costs")).toDouble(), 23.45);
    }

    void test_writes_multiple_paths_preserving_order()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        QList<PSR> results;
        for (int i = 0; i < 3; ++i)
        {
            PSR r;
            r.pathId    = i;
            r.pathUid   = QStringLiteral("uid-%1").arg(i);
            r.canonicalPathKey = r.pathUid;
            r.totalCost = 10.0 * i;
            results.append(r);
        }

        Cli::JsonResultsWriter w;
        QString                err;
        QVERIFY2(w.write(out, results, &err), qPrintable(err));

        const auto paths =
            readBack(out).value(QStringLiteral("paths")).toArray();
        QCOMPARE(paths.size(), 3);
        for (int i = 0; i < 3; ++i)
        {
            QCOMPARE(paths[i].toObject()
                         .value(QStringLiteral("path_id")).toInt(),
                     i);
        }
    }

    void test_empty_results_produces_valid_json_with_empty_paths_array()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        Cli::JsonResultsWriter w;
        QString                err;
        QVERIFY2(w.write(out, {}, &err), qPrintable(err));

        const QJsonObject root = readBack(out);
        QVERIFY(!root.isEmpty());   // still a well-formed document
        QVERIFY(root.value(QStringLiteral("paths")).isArray());
        QCOMPARE(root.value(QStringLiteral("paths")).toArray().size(), 0);
    }

    void test_writes_schema_version_and_utc_generated_at_metadata()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        Cli::JsonResultsWriter w;
        QString                err;
        QVERIFY2(w.write(out, {}, &err), qPrintable(err));

        const QJsonObject root = readBack(out);
        QCOMPARE(root.value(QStringLiteral("schema_version")).toInt(), 2);

        // ISO 8601 UTC: "YYYY-MM-DDTHH:MM:SSZ". The trailing Z is the
        // contract — downstream parsers depend on the explicit UTC
        // indicator. Use a regex rather than exact match because the
        // clock value is not deterministic.
        const QString ts =
            root.value(QStringLiteral("generated_at")).toString();
        const QRegularExpression isoUtc(
            QStringLiteral(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)"));
        QVERIFY2(isoUtc.match(ts).hasMatch(), qPrintable(ts));
    }

    void test_writes_optional_metrics_block_when_provided()
    {
        using namespace CargoNetSim;
        using namespace CargoNetSim::Backend::Scenario;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        PSR r;
        r.pathId    = 42;
        r.pathUid   = QStringLiteral("uid-O1-DA-r0");
        r.canonicalPathKey = r.pathUid;
        r.totalCost = 1.0;

        PathMetrics m;
        m.valid               = true;
        m.containerCount      = 6;
        m.vehiclesNeeded      = 1;
        m.previewVehicleBreakdown = {
            PathMetrics::VehicleRequirement{
                0,
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train,
                1}};
        m.distanceKm          = 120.0;
        m.travelTimeHours     = 2.0;
        m.fuelPerVehicle      = 2.5;
        m.energyPerVehicle    = 3000.0;
        m.carbonPerVehicle    = 0.804;
        m.riskPerVehicle      = 0.02;
        m.fuelPerContainer    = 0.417;
        m.energyPerContainer  = 500.0;
        m.carbonPerContainer  = 0.134;
        m.riskPerContainer    = 0.003;
        m.fuelType            = QStringLiteral("diesel_1");
        QHash<QString, PathMetrics> metrics;
        metrics.insert(r.canonicalPathKey, m);

        PathKey key{ QStringLiteral("O1"), QStringLiteral("DA"), 0 };
        QHash<QString, PathKey> keys;
        keys.insert(r.canonicalPathKey, key);

        Cli::JsonResultsWriter w;
        QString                err;
        QVERIFY2(w.write(out, {r}, &err, metrics, keys), qPrintable(err));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(f.readAll()).object();
        const auto paths = root.value(QStringLiteral("paths")).toArray();
        QCOMPARE(paths.size(), 1);
        const auto pObj = paths.first().toObject();
        QCOMPARE(pObj.value(QStringLiteral("origin")).toString(),
                 QString("O1"));
        QCOMPARE(pObj.value(QStringLiteral("destination")).toString(),
                 QString("DA"));
        QCOMPARE(pObj.value(QStringLiteral("rank")).toInt(), 0);
        QCOMPARE(pObj.value(QStringLiteral("path_uid")).toString(),
                 r.pathUid);
        const auto mo = pObj.value(QStringLiteral("metrics")).toObject();
        QCOMPARE(mo.value(QStringLiteral("preview_container_count")).toInt(), 6);
        QCOMPARE(mo.value(QStringLiteral("preview_vehicles_needed")).toInt(), 1);
        const auto breakdown =
            mo.value(QStringLiteral("preview_vehicle_breakdown"))
                .toArray();
        QCOMPARE(breakdown.size(), 1);
        QCOMPARE(breakdown.first()
                     .toObject()
                     .value(QStringLiteral("mode_name"))
                     .toString(),
                 QStringLiteral("Train"));
        QCOMPARE(mo.value(QStringLiteral("container_count")).toInt(), 6);
        QCOMPARE(mo.value(QStringLiteral("vehicles_needed")).toInt(), 1);
        QCOMPARE(mo.value(QStringLiteral("per_vehicle")).toObject()
                     .value(QStringLiteral("fuel")).toDouble(),
                 2.5);
        QCOMPARE(mo.value(QStringLiteral("per_container")).toObject()
                     .value(QStringLiteral("energy_kwh")).toDouble(),
                 500.0);
    }

    void test_ranked_but_unsimulated_alternative_is_labeled_explicitly()
    {
        QSKIP("Deferred outside Phase 4: explicit simulation_state for ranked but undispatched alternatives depends on the alternative-path simulation policy work.");

        using namespace CargoNetSim;
        using namespace CargoNetSim::Backend::Scenario;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        PSR dispatched;
        dispatched.pathId = 1;
        dispatched.pathUid = QStringLiteral("uid-1");
        dispatched.canonicalPathKey = dispatched.pathUid;
        dispatched.totalCost = 100.0;

        PSR alternative;
        alternative.pathId = 2;
        alternative.pathUid = QStringLiteral("uid-2");
        alternative.canonicalPathKey = alternative.pathUid;
        alternative.totalCost = 120.0;

        PathMetrics simulated;
        simulated.valid = true;
        simulated.containerCount = 6;
        simulated.vehiclesNeeded = 1;

        QHash<QString, PathMetrics> metrics;
        metrics.insert(QStringLiteral("uid-1"), simulated);
        metrics.insert(QStringLiteral("uid-2"), PathMetrics{});

        QHash<QString, PathKey> keys;
        keys.insert(QStringLiteral("uid-1"),
                    PathKey{QStringLiteral("O1"), QStringLiteral("DA"), 0});
        keys.insert(QStringLiteral("uid-2"),
                    PathKey{QStringLiteral("O1"), QStringLiteral("DA"), 1});

        Cli::JsonResultsWriter w;
        QString err;
        QVERIFY2(w.write(out, {dispatched, alternative}, &err, metrics, keys),
                 qPrintable(err));

        const auto root = readBack(out);
        const auto paths = root.value(QStringLiteral("paths")).toArray();
        QCOMPARE(paths.size(), 2);

        const auto altObj = paths[1].toObject();
        QCOMPARE(altObj.value(QStringLiteral("rank")).toInt(), 1);
        QCOMPARE(altObj.value(QStringLiteral("simulation_state")).toString(),
                 QStringLiteral("not_simulated"));
    }

    void test_duplicate_display_path_ids_do_not_collapse_canonical_metrics()
    {
        using namespace CargoNetSim;
        using namespace CargoNetSim::Backend::Scenario;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.json"));

        PSR first;
        first.pathId = 1;
        first.pathUid = QStringLiteral("uid-O1-DA-r0");
        first.canonicalPathKey = first.pathUid;
        first.originId = QStringLiteral("O1");
        first.destinationId = QStringLiteral("DA");
        first.rank = 0;
        first.effectiveContainerCount = 4;
        first.totalCost = 100.0;

        PSR second;
        second.pathId = 1;
        second.pathUid = QStringLiteral("uid-O2-DB-r0");
        second.canonicalPathKey = second.pathUid;
        second.originId = QStringLiteral("O2");
        second.destinationId = QStringLiteral("DB");
        second.rank = 0;
        second.effectiveContainerCount = 7;
        second.totalCost = 200.0;

        PathMetrics firstMetrics;
        firstMetrics.valid = true;
        firstMetrics.containerCount = 4;

        PathMetrics secondMetrics;
        secondMetrics.valid = true;
        secondMetrics.containerCount = 7;

        QHash<QString, PathMetrics> metrics;
        metrics.insert(first.canonicalPathKey, firstMetrics);
        metrics.insert(second.canonicalPathKey, secondMetrics);

        QHash<QString, PathKey> keys;
        keys.insert(first.canonicalPathKey,
                    PathKey{QStringLiteral("O1"), QStringLiteral("DA"), 0});
        keys.insert(second.canonicalPathKey,
                    PathKey{QStringLiteral("O2"), QStringLiteral("DB"), 0});

        Cli::JsonResultsWriter w;
        QString err;
        QVERIFY2(w.write(out, {first, second}, &err, metrics, keys),
                 qPrintable(err));

        const auto paths =
            readBack(out).value(QStringLiteral("paths")).toArray();
        QCOMPARE(paths.size(), 2);

        const auto firstObj = paths[0].toObject();
        const auto secondObj = paths[1].toObject();
        QCOMPARE(firstObj.value(QStringLiteral("path_id")).toInt(), 1);
        QCOMPARE(secondObj.value(QStringLiteral("path_id")).toInt(), 1);
        QCOMPARE(firstObj.value(QStringLiteral("path_uid")).toString(),
                 first.pathUid);
        QCOMPARE(secondObj.value(QStringLiteral("path_uid")).toString(),
                 second.pathUid);
        QCOMPARE(firstObj.value(QStringLiteral("metrics")).toObject()
                     .value(QStringLiteral("container_count")).toInt(),
                 4);
        QCOMPARE(secondObj.value(QStringLiteral("metrics")).toObject()
                     .value(QStringLiteral("container_count")).toInt(),
                 7);
        QCOMPARE(firstObj.value(QStringLiteral("origin")).toString(),
                 QStringLiteral("O1"));
        QCOMPARE(secondObj.value(QStringLiteral("origin")).toString(),
                 QStringLiteral("O2"));
    }
};

QTEST_MAIN(JsonResultsWriterTest)
#include "JsonResultsWriterTest.moc"
