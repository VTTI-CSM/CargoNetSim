#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QString>
#include <QTest>

// Plan 5 Task 19: end-to-end snapshot test for `cargonetsim-cli --help`.
//
// Pins the byte-for-byte equivalence between:
//   * `tools/cli/docs/help.txt` (the committed help text — single
//     source of truth that ships in the AUTORCC resource), and
//   * the standard output of the production `cargonetsim-cli`
//     binary invoked with `--help`.
//
// Any drift between the two — accidental edit to the resource, a
// regression in HelpCommand, a moc/AUTORCC misconfiguration — fails
// loudly here. HelpVersionTest (Task 11) covers the HelpCommand
// class via QBuffer injection; this snapshot covers the FULL chain
// from argv → dispatcher → HelpCommand → resource → stdout that an
// end user actually sees.
//
// The binary path is injected by the test target's
// `target_compile_definitions` (CARGONETSIM_CLI_BINARY) so the test
// does not guess at CMake build-layout-relative paths. The committed
// help.txt is located via `QFINDTESTDATA` (relative to the test's
// source directory), which Qt resolves through both source and
// build trees.
//
// Note on environment: spawning `cargonetsim-cli` requires the same
// runtime environment any direct invocation does (Qt libraries on
// the loader path). QProcess inherits the parent's environment by
// default — the test runner must already have a working
// LD_LIBRARY_PATH (Linux) / Path (Windows) for Qt, exactly the
// way every other test that links Qt runs.

class HelpTextSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void test_binary_help_output_matches_committed_help_txt()
    {
        // Locate the committed help text. QFINDTESTDATA searches both
        // the source tree (relative to this .cpp) and a few build-
        // relative paths.
        const QString committedPath =
            QFINDTESTDATA(QStringLiteral(
                "../../src/CLI/docs/help.txt"));
        QVERIFY2(!committedPath.isEmpty(),
                 "Cannot locate src/CLI/docs/help.txt — "
                 "QFINDTESTDATA returned empty.");

        QFile committed(committedPath);
        QVERIFY2(committed.open(QIODevice::ReadOnly),
                 qPrintable(committed.errorString()));
        const QByteArray want = committed.readAll();
        QVERIFY(!want.isEmpty());

        // Run the production binary with --help. The CARGONETSIM_CLI_BINARY
        // macro is a CMake-generated absolute path to the built target,
        // so the test does not depend on the working directory.
        QProcess p;
        p.start(QStringLiteral(CARGONETSIM_CLI_BINARY),
                {QStringLiteral("--help")});
        QVERIFY2(p.waitForFinished(5000),
                 qPrintable(p.errorString()));
        QCOMPARE(p.exitCode(), 0);

        const QByteArray got = p.readAllStandardOutput();
        QCOMPARE(got, want);
    }
};

QTEST_MAIN(HelpTextSnapshotTest)
#include "HelpTextSnapshotTest.moc"
