#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

// Plan 5 Task 20: end-to-end integration test for `cargonetsim-cli run`.
//
// Gated on CARGONETSIM_INTEGRATION=1. The full pipeline requires:
//   * RabbitMQ broker reachable at the project's default URL,
//   * TerminalSim, ShipSim, TrainSim, TruckSim simulator processes
//     (or their daemonised equivalents) connected to that broker,
//   * the cargonetsim-cli binary built (handled by `add_dependencies`).
//
// Without those prerequisites the test self-skips so CI / dev
// machines without live simulators stay green. With them, this test
// verifies the WHOLE chain from `run scenario.yml` argv → parse →
// validate → controller startAll → ScenarioRuntime → PathDiscovery
// → IPC handler → executor → results.json on disk.
//
// The CLI binary's absolute path is injected at compile time via the
// `CARGONETSIM_CLI_BINARY` macro (see tests/CLI/CMakeLists.txt — same
// pattern Task 19 uses for HelpTextSnapshotTest).

class EndToEndRunTest : public QObject
{
    Q_OBJECT

private slots:
    void test_run_subcommand_emits_results_json_against_live_simulators()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live simulators "
                  "(set CARGONETSIM_INTEGRATION=1)");

        // Locate the CLI-specific fixture (Task 0 properties present).
        const QString fixturePath = QFINDTESTDATA(
            QStringLiteral("../../tests/fixtures/cli/cli_run_scenario.yml"));
        QVERIFY2(!fixturePath.isEmpty(),
                 "Cannot locate cli_run_scenario.yml — "
                 "QFINDTESTDATA returned empty.");

        // The YAML's output.directory is "./results" — relative to
        // CWD. We cd into a temp dir so the writer lands its file
        // somewhere this test owns; the qScopeGuard restores the
        // original CWD on EVERY exit path so a failed assertion
        // does not leak global state into subsequent tests in the
        // same process.
        QTemporaryDir workDir;
        QVERIFY(workDir.isValid());
        const QString savedCwd = QDir::currentPath();
        auto          cwdGuard = qScopeGuard(
                     [&savedCwd] { QDir::setCurrent(savedCwd); });
        QVERIFY(QDir::setCurrent(workDir.path()));

        // Run the binary with the absolute fixture path. Five-minute
        // budget bounds the wait but is large enough for a real
        // multi-segment simulation through the four simulator
        // processes (terminal/ship/train/truck) on a development
        // machine.
        QProcess p;
        p.start(QStringLiteral(CARGONETSIM_CLI_BINARY),
                {QStringLiteral("run"), fixturePath});
        QVERIFY2(p.waitForFinished(5 * 60 * 1000),
                 qPrintable(p.errorString()));
        QCOMPARE(p.exitCode(), 0);

        // The YAML declares `output.directory: ./results` and
        // `formats: [json]`, so success means a results.json file
        // appeared under workDir/results/. JsonResultsWriter (Task 6)
        // is responsible for the file's actual shape — locked
        // separately by JsonResultsWriterTest.
        const QString resultsPath =
            QDir(workDir.path()).filePath(
                QStringLiteral("results/results.json"));
        QVERIFY2(QFile::exists(resultsPath),
                 qPrintable(QString("Expected %1, got nothing")
                                .arg(resultsPath)));
    }
};

QTEST_MAIN(EndToEndRunTest)
#include "EndToEndRunTest.moc"
