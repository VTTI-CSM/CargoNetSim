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
        r.totalCost = 1.0;

        PathMetrics m;
        m.valid               = true;
        m.containerCount      = 6;
        m.vehiclesNeeded      = 1;
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
        QHash<int, PathMetrics> metrics;
        metrics.insert(42, m);

        PathKey key{ QStringLiteral("O1"), QStringLiteral("DA"), 0 };
        QHash<int, PathKey> keys;
        keys.insert(42, key);

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
        const auto mo = pObj.value(QStringLiteral("metrics")).toObject();
        QCOMPARE(mo.value(QStringLiteral("container_count")).toInt(), 6);
        QCOMPARE(mo.value(QStringLiteral("vehicles_needed")).toInt(), 1);
        QCOMPARE(mo.value(QStringLiteral("per_vehicle")).toObject()
                     .value(QStringLiteral("fuel")).toDouble(),
                 2.5);
        QCOMPARE(mo.value(QStringLiteral("per_container")).toObject()
                     .value(QStringLiteral("energy_kwh")).toDouble(),
                 500.0);
    }
};

QTEST_MAIN(JsonResultsWriterTest)
#include "JsonResultsWriterTest.moc"
