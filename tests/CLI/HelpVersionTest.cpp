#include <QBuffer>
#include <QByteArray>
#include <QTest>

#include "Version.h"
#include "CLI/Commands/HelpVersion.h"
#include "CLI/ExitCodes.h"

// Lock-tests for HelpCommand and VersionCommand (Plan 5 Task 11).
//
// Each test injects a QBuffer as the output sink so the emitted bytes
// can be inspected without process spawning. Production code uses the
// default constructor (stdout) — the QBuffer overload is exclusively
// a test seam.

class HelpVersionTest : public QObject
{
    Q_OBJECT

private slots:
    void test_help_command_writes_help_text_with_expected_sections()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::HelpCommand cmd(&sink);
        const int rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));

        // The help text is the published contract — assert the
        // section headers downstream tooling and users grep for.
        const QByteArray bytes = sink.data();
        QVERIFY2(bytes.contains("USAGE"),       "missing USAGE header");
        QVERIFY2(bytes.contains("SUBCOMMANDS"), "missing SUBCOMMANDS header");
        QVERIFY2(bytes.contains("EXIT CODES"),  "missing EXIT CODES header");
        QVERIFY2(bytes.contains("ENVIRONMENT"), "missing ENVIRONMENT header");
    }

    void test_version_command_writes_cargonetsim_cli_version_line()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::VersionCommand cmd(&sink);
        const int rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));

        // Single source of truth for the version: CARGONETSIM_VERSION
        // macro from the CMake-generated Version.h. The line format
        // is locked: "cargonetsim-cli <version>\n".
        const QByteArray expected =
            QByteArray("cargonetsim-cli ") + CARGONETSIM_VERSION + "\n";
        QCOMPARE(sink.data(), expected);
    }
};

QTEST_MAIN(HelpVersionTest)
#include "HelpVersionTest.moc"
