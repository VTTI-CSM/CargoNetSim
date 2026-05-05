#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "CLI/Commands/PreviewCommand.h"
#include "CLI/ExitCodes.h"

// Lock-tests for PreviewCommand (Plan 5 Task 14).
//
// Fixtures are copied next to the binary via the POST_BUILD rule in
// tests/CLI/CMakeLists.txt so paths resolve against
// QCoreApplication::applicationDirPath().

class PreviewCommandTest : public QObject
{
    Q_OBJECT

private:
    static QString fixture(const QString &name)
    {
        return QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("fixtures/scenario/") + name);
    }

private slots:
    void test_missing_argument_writes_error_and_returns_bad_args()
    {
        using namespace CargoNetSim;
        QBuffer out, err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PreviewCommand cmd(&out, &err);
        const int           rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains("missing scenario.yml"));
    }

    void test_nonexistent_file_returns_validation_failed_with_parse_error()
    {
        using namespace CargoNetSim;
        QBuffer out, err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PreviewCommand cmd(&out, &err);
        const int           rc = cmd.execute(
            {QStringLiteral("/absolutely/does/not/exist/scenario.yml")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::ValidationFailed));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains("failed to parse"));
    }

    void test_validator_error_fixture_returns_validation_failed_no_stdout()
    {
        using namespace CargoNetSim;
        QBuffer out, err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PreviewCommand cmd(&out, &err);
        const int           rc = cmd.execute(
            {fixture(QStringLiteral("invalid_bad_terminal_type.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::ValidationFailed));
        // No JSON emitted — we never reached the toJson step.
        QVERIFY(out.data().isEmpty());
        // Issue surfaced to stderr via the shared IssueFormatter.
        QVERIFY(err.data().contains("ERROR"));
        QVERIFY(err.data().contains("terminals[T1]"));
    }

    void test_minimal_fixture_emits_parseable_indented_json_on_stdout()
    {
        using namespace CargoNetSim;
        QBuffer out, err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PreviewCommand cmd(&out, &err);
        const int           rc = cmd.execute(
            {fixture(QStringLiteral("minimal.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        QVERIFY(err.data().isEmpty());

        // The stdout payload must be parseable JSON with the
        // document's top-level keys present (even if the arrays are
        // empty for the minimal fixture). This locks the published
        // output contract without pinning the exact field values.
        const auto doc = QJsonDocument::fromJson(out.data());
        QVERIFY(doc.isObject());
        const auto root = doc.object();
        QVERIFY(root.contains(QStringLiteral("regions")));
        QVERIFY(root.contains(QStringLiteral("terminals")));
        QVERIFY(root.contains(QStringLiteral("connections")));
    }
};

QTEST_MAIN(PreviewCommandTest)
#include "PreviewCommandTest.moc"
