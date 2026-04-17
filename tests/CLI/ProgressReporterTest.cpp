#include <QBuffer>
#include <QByteArray>
#include <QTest>

#include "CLI/Progress/ProgressReporter.h"

// Contract lock-tests for ProgressReporter (Plan 5 Task 8).
//
// The reporter is tested with an injected QBuffer as the output sink
// so each test can count and inspect emissions deterministically
// without touching stderr or manipulating file descriptors. Production
// paths use the default constructor (stderr) — the QBuffer overload is
// exclusively a test seam.
//
// The 2-second wall-clock rule is NOT tested here: exercising it
// deterministically would require abstracting QElapsedTimer, which is
// scope creep for a single rule. Task 20's end-to-end integration test
// covers the 2s-path under real wall time.

class ProgressReporterTest : public QObject
{
    Q_OBJECT

private:
    /// Count tick lines in @p bytes by counting newline terminators —
    /// every emission ends in exactly one `\n`, so line count equals
    /// emission count.
    static int countTicks(const QByteArray &bytes)
    {
        return bytes.count('\n');
    }

private slots:
    void test_first_call_always_emits_regardless_of_percent()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/false, &sink);
        r.report(/*time=*/0.0, /*percent=*/0.0);

        QCOMPARE(countTicks(sink.data()), 1);
        QVERIFY(sink.data().contains("[  0.0%]"));
    }

    void test_emits_exactly_once_per_five_percent_increment_in_tight_loop()
    {
        // 100 inputs at 1% spacing, tight loop (<< 2 seconds), so only
        // the 5%-rule is relevant — the 2s-rule never fires here.
        // Expected emissions: at 0, 5, 10, …, 95 → exactly 20.
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/false, &sink);
        for (int i = 0; i < 100; ++i)
            r.report(/*time=*/static_cast<double>(i), /*percent=*/i);

        QCOMPARE(countTicks(sink.data()), 20);
    }

    void test_stalled_progress_does_not_re_emit_within_rate_window()
    {
        // Multiple reports at the same percent: only the first fires
        // (initial-call rule); none of the later calls cross another
        // 5%-boundary, and the tight loop stays under 2s.
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/false, &sink);
        for (int i = 0; i < 50; ++i)
            r.report(/*time=*/42.0, /*percent=*/12.5);

        QCOMPARE(countTicks(sink.data()), 1);
    }

    void test_quiet_mode_suppresses_every_report()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/true, &sink);
        for (int i = 0; i < 100; ++i)
            r.report(static_cast<double>(i), i);

        QCOMPARE(countTicks(sink.data()), 0);
    }

    void test_emit_final_always_emits_with_done_marker()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/false, &sink);
        // No prior report() — emitFinal() must still fire.
        r.emitFinal(/*percent=*/100.0);

        QCOMPARE(countTicks(sink.data()), 1);
        QVERIFY(sink.data().contains("done"));
        QVERIFY(sink.data().contains("[100.0%]"));
    }

    void test_emit_final_also_respects_quiet_mode()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ProgressReporter r(/*quiet=*/true, &sink);
        r.emitFinal(100.0);

        QCOMPARE(countTicks(sink.data()), 0);
    }
};

QTEST_MAIN(ProgressReporterTest)
#include "ProgressReporterTest.moc"
