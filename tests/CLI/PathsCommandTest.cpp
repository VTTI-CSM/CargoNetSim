#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QIODevice>
#include <QTest>

#include "CLI/Commands/PathsCommand.h"
#include "CLI/ExitCodes.h"

class PathsCommandTest : public QObject
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
        QBuffer out;
        QBuffer err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PathsCommand cmd(&out, &err);
        const int rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains(
            "expected exactly one scenario argument"));
    }

    void test_invalid_top_value_returns_bad_args()
    {
        using namespace CargoNetSim;
        QBuffer out;
        QBuffer err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PathsCommand cmd(&out, &err);
        const int rc = cmd.execute(
            {QStringLiteral("--top"), QStringLiteral("0"),
             fixture(QStringLiteral("minimal.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains(
            "--top requires a positive integer"));
    }

    void test_json_and_details_are_mutually_exclusive()
    {
        using namespace CargoNetSim;
        QBuffer out;
        QBuffer err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PathsCommand cmd(&out, &err);
        const int rc = cmd.execute(
            {QStringLiteral("--json"),
             QStringLiteral("--details"),
             fixture(QStringLiteral("minimal.yml"))});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains(
            "--json and --details cannot be used together"));
    }

    void test_nonexistent_file_returns_validation_failed()
    {
        using namespace CargoNetSim;
        QBuffer out;
        QBuffer err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PathsCommand cmd(&out, &err);
        const int rc = cmd.execute(
            {QStringLiteral("/absolutely/does/not/exist/scenario.yml")});

        QCOMPARE(rc,
                 static_cast<int>(Cli::ExitCode::ValidationFailed));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains("failed to parse"));
    }

    void test_invalid_fixture_returns_validation_failed_without_stdout()
    {
        using namespace CargoNetSim;
        QBuffer out;
        QBuffer err;
        QVERIFY(out.open(QIODevice::WriteOnly));
        QVERIFY(err.open(QIODevice::WriteOnly));

        Cli::PathsCommand cmd(&out, &err);
        const int rc = cmd.execute(
            {fixture(QStringLiteral("invalid_bad_terminal_type.yml"))});

        QCOMPARE(rc,
                 static_cast<int>(Cli::ExitCode::ValidationFailed));
        QVERIFY(out.data().isEmpty());
        QVERIFY(err.data().contains("ERROR"));
        QVERIFY(err.data().contains("terminals[T1]"));
    }
};

QTEST_MAIN(PathsCommandTest)
#include "PathsCommandTest.moc"
