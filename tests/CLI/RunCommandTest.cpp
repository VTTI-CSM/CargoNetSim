#include <QBuffer>
#include <QTest>

#include "Backend/Scenario/ScenarioDocument.h"
#include "CLI/Commands/RunCommand.h"
#include "CLI/ExitCodes.h"

class RunCommandTest : public QObject
{
    Q_OBJECT

private slots:
    void test_missing_scenario_argument_returns_bad_args()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        const int rc = cmd.execute({});

        QCOMPARE(rc,
                 static_cast<int>(
                     CargoNetSim::Cli::ExitCode::BadArgs));
        QVERIFY(
            sink.data().contains("expected exactly one scenario"));
    }

    void test_unsupported_flag_returns_bad_args()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        const int rc =
            cmd.execute({QStringLiteral("--path-rank"),
                         QStringLiteral("scenario.yml")});

        QCOMPARE(rc,
                 static_cast<int>(
                     CargoNetSim::Cli::ExitCode::BadArgs));
        QVERIFY(sink.data().contains("unsupported flag"));
        QVERIFY(sink.data().contains("--all"));
    }

    void test_bare_run_path_uses_transition_alias_for_all()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        const int rc =
            cmd.execute({QStringLiteral("does-not-exist.yml")});

        QCOMPARE(rc,
                 static_cast<int>(CargoNetSim::Cli::
                                      ExitCode::ValidationFailed));
        QVERIFY(
            sink.data().contains("failed to parse does-not-exist.yml"));
    }

    void test_explicit_all_flag_is_accepted()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        const int rc = cmd.execute(
            {QStringLiteral("--all"),
             QStringLiteral("does-not-exist.yml")});

        QCOMPARE(rc,
                 static_cast<int>(CargoNetSim::Cli::
                                      ExitCode::ValidationFailed));
        QVERIFY(
            sink.data().contains("failed to parse does-not-exist.yml"));
    }

    void test_write_outputs_fails_when_output_directory_creation_fails()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        CargoNetSim::Backend::Scenario::ScenarioDocument doc;
        doc.output.directory = QStringLiteral("/blocked");
        doc.output.formats = {QStringLiteral("json")};

        CargoNetSim::Cli::RunCommand::WriterHooks hooks =
            CargoNetSim::Cli::RunCommand::defaultWriterHooks();
        hooks.mkpath = [](const QString &) { return false; };

        const int rc = cmd.writeOutputs(doc, {}, {},
                                        /*predictedMetrics=*/{},
                                        /*pathKeys=*/{},
                                        hooks);

        QCOMPARE(rc,
                 static_cast<int>(CargoNetSim::Cli::
                                      ExitCode::RunFailed));
        QVERIFY(
            sink.data().contains("failed to create output directory"));
    }

    void test_write_outputs_returns_non_zero_when_writer_fails()
    {
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        CargoNetSim::Cli::RunCommand cmd(&sink);
        CargoNetSim::Backend::Scenario::ScenarioDocument doc;
        doc.output.directory = QStringLiteral("out");
        doc.output.formats = {QStringLiteral("json"),
                              QStringLiteral("csv")};

        CargoNetSim::Cli::RunCommand::WriterHooks hooks =
            CargoNetSim::Cli::RunCommand::defaultWriterHooks();
        hooks.mkpath = [](const QString &) { return true; };
        hooks.writeJson = [](
                              const QString &,
                              const QList<CargoNetSim::Backend::Scenario::
                                              PathSimulationResult> &,
                              QString *err,
                              const QHash<QString,
                                          CargoNetSim::Backend::Scenario::
                                              PathMetrics> &,
                              const QHash<QString,
                                          CargoNetSim::Backend::Scenario::
                                              PathKey> &,
                              const QList<CargoNetSim::Backend::Path *> &) {
            if (err)
                *err = QStringLiteral("json failed");
            return false;
        };
        hooks.writeCsv = [](
                             const QString &,
                             const QList<CargoNetSim::Backend::Scenario::
                                             PathSimulationResult> &,
                             QString *) { return true; };

        const int rc = cmd.writeOutputs(doc, {}, {},
                                        /*predictedMetrics=*/{},
                                        /*pathKeys=*/{},
                                        hooks);

        QCOMPARE(rc,
                 static_cast<int>(CargoNetSim::Cli::
                                      ExitCode::RunFailed));
        QVERIFY(sink.data().contains("writer 'json': json failed"));
    }
};

QTEST_MAIN(RunCommandTest)
#include "RunCommandTest.moc"
