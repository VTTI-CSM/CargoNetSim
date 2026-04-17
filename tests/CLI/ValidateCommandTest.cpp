#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QTest>

#include "CLI/Commands/ValidateCommand.h"
#include "CLI/ExitCodes.h"

// Lock-tests for ValidateCommand (Plan 5 Task 12).
//
// Fixtures are copied next to the test binary by the POST_BUILD rule
// in tests/CLI/CMakeLists.txt (same pattern ScenarioApplierTest uses),
// so paths resolve relative to `QCoreApplication::applicationDirPath()`.
// Each test captures stderr via an injected QBuffer to verify the
// emitted issue-line format without process spawning.

class ValidateCommandTest : public QObject
{
    Q_OBJECT

private:
    /// Path to a fixture file under `tests/fixtures/scenario/`, copied
    /// to `build/bin/fixtures/` at build time.
    static QString fixture(const QString &name)
    {
        return QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("fixtures/scenario/") + name);
    }

private slots:
    void test_missing_argument_writes_error_and_returns_bad_args()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ValidateCommand cmd(&sink);
        const int            rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(sink.data().contains("missing scenario.yml"));
    }

    void test_nonexistent_file_returns_validation_failed_with_parse_error_msg()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ValidateCommand cmd(&sink);
        const int            rc = cmd.execute(
            {QStringLiteral("/absolutely/does/not/exist/scenario.yml")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::ValidationFailed));
        QVERIFY(sink.data().contains("failed to parse"));
    }

    void test_valid_minimal_fixture_returns_success_with_no_output()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ValidateCommand cmd(&sink);
        const int            rc = cmd.execute(
            {fixture(QStringLiteral("minimal.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        // Clean YAML → no issues emitted at all.
        QVERIFY(sink.data().isEmpty());
    }

    void test_bad_terminal_type_fixture_returns_validation_failed()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ValidateCommand cmd(&sink);
        const int            rc = cmd.execute(
            {fixture(QStringLiteral("invalid_bad_terminal_type.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::ValidationFailed));
        // The fixture's offending terminal is `T1` with an unknown
        // `type: "Fictional Terminal Kind"`. Assert both pieces show
        // up in the ERROR line without locking the message verbatim.
        QVERIFY(sink.data().contains("ERROR"));
        QVERIFY(sink.data().contains("T1"));
    }

    void test_issue_line_format_is_severity_then_path_then_message()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ValidateCommand cmd(&sink);
        cmd.execute({fixture(QStringLiteral("invalid_bad_terminal_type.yml"))});

        // Expected shape per-line:
        //   "ERROR  terminals[T1].type: <msg>\n"
        // The validator reports the dotted path down to the offending
        // field (`.type` in this fixture's case). Locking the exact
        // prefix + path + trailing LF so the published format
        // (consumed by CI filters and shell scripts) is stable.
        const QByteArray data = sink.data();
        QVERIFY(data.startsWith("ERROR  terminals[T1].type:"));
        QVERIFY(data.endsWith("\n"));
    }
};

QTEST_MAIN(ValidateCommandTest)
#include "ValidateCommandTest.moc"
