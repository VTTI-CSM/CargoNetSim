#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <memory>

#include "CLI/Commands/StatusCommand.h"
#include "CLI/ExitCodes.h"
#include "CLI/Ipc/ControlChannel.h"
#include "CLI/Ipc/RuntimeStateFile.h"

// Integration-level lock-tests for StatusCommand (Plan 5 Task 15).
//
// Each test:
//   1. points CARGONETSIM_CLI_RUNTIME_DIR at a fresh QTemporaryDir,
//   2. (for live-server tests) spins up a real ControlChannelServer
//      locally with a canned handler,
//   3. writes a RuntimeStateEntry pointing at that server's socket,
//   4. runs StatusCommand with an injected QBuffer sink,
//   5. asserts the contract on exit code + captured output.
//
// The real-filesystem + real-socket approach tests the integration
// between Task 10 (state file) and Task 9 (control channel) as well
// as Task 15's own formatting.

class StatusCommandTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    static constexpr auto kEnvName = "CARGONETSIM_CLI_RUNTIME_DIR";

    /// PID-scoped server name + a unique slug so parallel test-process
    /// invocations on the same host cannot collide.
    static QString uniqueName(const QString &slug)
    {
        return QStringLiteral("cargonetsim-status-test-%1-%2")
            .arg(slug)
            .arg(QCoreApplication::applicationPid());
    }

    /// Build a minimal entry pointing at @p socketName. The scenario
    /// path and timestamp are illustrative — scanners don't care.
    static CargoNetSim::Cli::RuntimeStateEntry makeEntry(
        qint64 pid, const QString &socketName)
    {
        CargoNetSim::Cli::RuntimeStateEntry e;
        e.pid          = pid;
        e.socketName   = socketName;
        e.scenarioPath = QStringLiteral("/tmp/sim-%1.yml").arg(pid);
        e.startedAt    = QDateTime(QDate(2026, 4, 14),
                                   QTime(10, 30, 0), QTimeZone::UTC);
        return e;
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        qputenv(kEnvName, m_dir->path().toLocal8Bit());
    }

    void cleanup()
    {
        qunsetenv(kEnvName);
        m_dir.reset();
    }

    void test_no_state_files_returns_no_running_sim()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::StatusCommand cmd(&sink);
        const int          rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::NoRunningSim));
        QVERIFY(sink.data().contains("no running simulation"));
    }

    void test_live_server_responds_with_status_and_returns_success()
    {
        using namespace CargoNetSim;

        // Stand up a real server answering {"op":"status"} with a
        // plausible running-sim snapshot — identical shape to what
        // Task 17's RunCommand will emit.
        const QString serverName = uniqueName(QStringLiteral("live"));
        Cli::ControlChannelServer server;
        server.setHandler([](const QJsonObject &req) -> QJsonObject {
            if (req.value(QStringLiteral("op")).toString()
                != QLatin1String("status"))
                return QJsonObject{{QStringLiteral("error"),
                                    QStringLiteral("bad op")}};
            return QJsonObject{
                {QStringLiteral("state"),        QStringLiteral("running")},
                {QStringLiteral("current_time"), 14420.5},
                {QStringLiteral("end_time"),     86400.0},
                {QStringLiteral("progress"),     16.7},
            };
        });
        QVERIFY(server.listen(serverName));

        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(/*pid=*/1234, serverName), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StatusCommand cmd(&sink);
        const int          rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        // The plan's explicit assertion — lock the human-format
        // column alignment for the `progress` line.
        QVERIFY2(sink.data().contains("progress:     16.7%"),
                 sink.data().constData());
    }

    void test_json_flag_emits_json_array_with_merged_fields()
    {
        using namespace CargoNetSim;

        const QString serverName = uniqueName(QStringLiteral("json"));
        Cli::ControlChannelServer server;
        server.setHandler([](const QJsonObject &) -> QJsonObject {
            return QJsonObject{
                {QStringLiteral("state"),        QStringLiteral("running")},
                {QStringLiteral("current_time"), 100.0},
                {QStringLiteral("end_time"),     1000.0},
                {QStringLiteral("progress"),     10.0},
            };
        });
        QVERIFY(server.listen(serverName));
        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(42, serverName), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StatusCommand cmd(&sink);
        const int          rc = cmd.execute({QStringLiteral("--json")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));

        // Parse back and verify the merge: the server's fields
        // coexist with the state-file fields on each element.
        const auto doc = QJsonDocument::fromJson(sink.data());
        QVERIFY(doc.isArray());
        const auto arr = doc.array();
        QCOMPARE(arr.size(), 1);

        const auto obj = arr.first().toObject();
        QCOMPARE(obj.value(QStringLiteral("pid")).toInteger(), qint64(42));
        QVERIFY(obj.contains(QStringLiteral("scenario_path")));
        QVERIFY(obj.contains(QStringLiteral("started_at")));
        QCOMPARE(obj.value(QStringLiteral("state")).toString(),
                 QStringLiteral("running"));
        QCOMPARE(obj.value(QStringLiteral("progress")).toDouble(), 10.0);
    }

    void test_stale_state_file_is_skipped_and_returns_no_running_sim()
    {
        using namespace CargoNetSim;

        // State file points at a socket no one is listening on.
        // ControlChannelClient times out; StatusCommand skips the
        // entry; overall result is NoRunningSim.
        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(999, uniqueName(QStringLiteral("stale"))),
            nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StatusCommand cmd(&sink);
        const int          rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::NoRunningSim));
        QVERIFY(sink.data().contains("no running simulation"));
    }

    void test_two_live_servers_both_reported()
    {
        using namespace CargoNetSim;

        const QString nameA = uniqueName(QStringLiteral("twoA"));
        const QString nameB = uniqueName(QStringLiteral("twoB"));

        Cli::ControlChannelServer serverA;
        Cli::ControlChannelServer serverB;
        auto responder = [](QString nameTag) {
            return [nameTag](const QJsonObject &) {
                return QJsonObject{
                    {QStringLiteral("state"),        nameTag},
                    {QStringLiteral("current_time"), 0.0},
                    {QStringLiteral("end_time"),     0.0},
                    {QStringLiteral("progress"),     0.0},
                };
            };
        };
        serverA.setHandler(responder(QStringLiteral("stateA")));
        serverB.setHandler(responder(QStringLiteral("stateB")));
        QVERIFY(serverA.listen(nameA));
        QVERIFY(serverB.listen(nameB));

        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(100, nameA), nullptr));
        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(200, nameB), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StatusCommand cmd(&sink);
        const int          rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        QVERIFY(sink.data().contains("[100]"));
        QVERIFY(sink.data().contains("[200]"));
        QVERIFY(sink.data().contains("stateA"));
        QVERIFY(sink.data().contains("stateB"));
    }
};

QTEST_MAIN(StatusCommandTest)
#include "StatusCommandTest.moc"
