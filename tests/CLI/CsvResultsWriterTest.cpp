#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include "Backend/Scenario/PathSimulationResult.h"
#include "CLI/Output/CsvResultsWriter.h"

// Contract lock-tests for CsvResultsWriter (Plan 5 Task 7).
//
// The emitted shape is a published contract (consumers parse these
// files from shell scripts, spreadsheets, and CI). Fields, ordering,
// number format, and line endings must not drift silently — each test
// below asserts one slice of that contract.

class CsvResultsWriterTest : public QObject
{
    Q_OBJECT

private:
    using PSR = CargoNetSim::Backend::Scenario::PathSimulationResult;

    /// Raw-byte read — returned unprocessed so line-ending tests can
    /// inspect for stray `\r` bytes the Text-mode translation would
    /// have introduced.
    QByteArray readBytes(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    /// Convenience: split on LF without any stripping so downstream
    /// assertions can count header + data rows accurately.
    QList<QByteArray> linesLf(const QByteArray &bytes)
    {
        return bytes.split('\n');
    }

private slots:
    void test_writes_header_plus_one_row_with_all_four_columns()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.csv"));

        PSR r;
        r.pathId        = 7;
        r.totalCost     = 123.45;
        r.edgeCosts     = 100.0;
        r.terminalCosts = 23.45;

        Cli::CsvResultsWriter w;
        QString               err;
        QVERIFY2(w.write(out, {r}, &err), qPrintable(err));

        const auto bytes = readBytes(out);
        const auto lines = linesLf(bytes);
        // Header + data row + trailing empty element (bytes ends with LF).
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], QByteArray("path_id,total_cost,edge_costs,terminal_costs"));
        QCOMPARE(lines[1], QByteArray("7,123.450000,100.000000,23.450000"));
        QVERIFY(lines[2].isEmpty());
    }

    void test_writes_lf_line_endings_only_no_crlf()
    {
        // Spec §9: LF-everywhere. On Windows, QIODevice::Text would
        // turn every \n into \r\n — this test is the regression lock
        // that caught the plan's original `Text` flag and forces the
        // writer to stay in binary mode.
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.csv"));

        PSR r; r.pathId = 1;
        Cli::CsvResultsWriter w;
        QString               err;
        QVERIFY2(w.write(out, {r}, &err), qPrintable(err));

        const auto bytes = readBytes(out);
        QVERIFY2(!bytes.contains('\r'),
                 "Carriage return found — writer must emit LF-only, "
                 "spec §9.");
        QVERIFY(bytes.endsWith('\n'));
    }

    void test_writes_multiple_rows_preserving_order()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.csv"));

        QList<PSR> results;
        for (int i = 0; i < 3; ++i)
        {
            PSR r; r.pathId = i; r.totalCost = 10.0 * i;
            results.append(r);
        }

        Cli::CsvResultsWriter w;
        QString               err;
        QVERIFY2(w.write(out, results, &err), qPrintable(err));

        const auto lines = linesLf(readBytes(out));
        QCOMPARE(lines.size(), 5);   // header + 3 rows + trailing empty
        QVERIFY(lines[1].startsWith("0,"));
        QVERIFY(lines[2].startsWith("1,"));
        QVERIFY(lines[3].startsWith("2,"));
    }

    void test_empty_results_produces_header_only_file()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.csv"));

        Cli::CsvResultsWriter w;
        QString               err;
        QVERIFY2(w.write(out, {}, &err), qPrintable(err));

        const auto bytes = readBytes(out);
        // Header line + trailing LF, nothing else.
        QCOMPARE(bytes,
                 QByteArray("path_id,total_cost,edge_costs,terminal_costs\n"));
    }

    void test_writes_doubles_with_six_decimal_places()
    {
        using namespace CargoNetSim;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString out = dir.filePath(QStringLiteral("results.csv"));

        PSR r;
        r.pathId        = 42;
        r.totalCost     = 1.0;         // exact integer → "1.000000"
        r.edgeCosts     = 0.123456789; // truncated → "0.123457"
        r.terminalCosts = 0.0;         // zero      → "0.000000"

        Cli::CsvResultsWriter w;
        QString               err;
        QVERIFY2(w.write(out, {r}, &err), qPrintable(err));

        const auto lines = linesLf(readBytes(out));
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[1],
                 QByteArray("42,1.000000,0.123457,0.000000"));
    }
};

QTEST_MAIN(CsvResultsWriterTest)
#include "CsvResultsWriterTest.moc"
