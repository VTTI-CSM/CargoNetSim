#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <memory>

#include "CLI/Commands/StopCommand.h"
#include "CLI/ExitCodes.h"
#include "CLI/Ipc/ControlChannel.h"
#include "CLI/Ipc/RuntimeStateFile.h"

// Integration-level lock-tests for StopCommand (Plan 5 Task 16).
//
// Same real-env pattern used by StatusCommandTest: QTemporaryDir +
// env override + in-process ControlChannelServer. No mocking — the
// scan ↔ state-file ↔ socket integration path is exercised.
//
// The "stale socket" path (10-second timeout) is intentionally NOT
// tested here to keep the suite fast. Task 9's ControlChannelTest
// already covers the client-side timeout behaviour, and Task 20's
// end-to-end exercises the combined flow under real wall time.

class StopCommandTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    static constexpr auto kEnvName = "CARGONETSIM_CLI_RUNTIME_DIR";

    static QString uniqueName(const QString &slug)
    {
        return QStringLiteral("cargonetsim-stop-test-%1-%2")
            .arg(slug)
            .arg(QCoreApplication::applicationPid());
    }

    static CargoNetSim::Cli::RuntimeStateEntry makeEntry(
        qint64 pid, const QString &socketName)
    {
        CargoNetSim::Cli::RuntimeStateEntry e;
        e.pid          = pid;
        e.socketName   = socketName;
        e.scenarioPath = QStringLiteral("/tmp/sim-%1.yml").arg(pid);
        e.startedAt    = QDateTime(QDate(2026, 4, 14),
                                   QTime(11, 0, 0), QTimeZone::UTC);
        return e;
    }

    /// Shared handler: replies `{"ok":true}` to every `{"op":"stop"}`
    /// request, mirroring what Task 17's RunCommand will do.
    static CargoNetSim::Cli::ControlChannelServer::Handler ackStopHandler()
    {
        return [](const QJsonObject &req) -> QJsonObject {
            if (req.value(QStringLiteral("op")).toString()
                != QLatin1String("stop"))
                return QJsonObject{{QStringLiteral("error"),
                                    QStringLiteral("bad op")}};
            return QJsonObject{{QStringLiteral("ok"), true}};
        };
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

    void test_no_state_files_returns_no_running_sim_with_diagnostic()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::NoRunningSim));
        QVERIFY(sink.data().contains("no running simulation"));
    }

    void test_single_entry_successful_stop_returns_success()
    {
        using namespace CargoNetSim;

        const QString             name = uniqueName(QStringLiteral("one"));
        Cli::ControlChannelServer server;
        server.setHandler(ackStopHandler());
        QVERIFY(server.listen(name));
        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(/*pid=*/1234, name), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        QVERIFY(sink.data().isEmpty());  // success path is silent
    }

    void test_multiple_entries_without_pid_returns_bad_args_with_pid_list()
    {
        using namespace CargoNetSim;

        // Two servers, two state files — `stop` cannot pick without
        // disambiguation.
        const QString nameA = uniqueName(QStringLiteral("multiA"));
        const QString nameB = uniqueName(QStringLiteral("multiB"));
        Cli::ControlChannelServer sA, sB;
        sA.setHandler(ackStopHandler());
        sB.setHandler(ackStopHandler());
        QVERIFY(sA.listen(nameA));
        QVERIFY(sB.listen(nameB));
        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(100, nameA), nullptr));
        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(200, nameB), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(sink.data().contains("multiple running simulations"));
        // The diagnostic must list the actual PIDs so the user can
        // pick — locked here so a future refactor does not reduce the
        // message to a generic error.
        QVERIFY(sink.data().contains("100"));
        QVERIFY(sink.data().contains("200"));
    }

    void test_multiple_entries_with_pid_matches_and_stops_target()
    {
        using namespace CargoNetSim;

        const QString nameA = uniqueName(QStringLiteral("tgtA"));
        const QString nameB = uniqueName(QStringLiteral("tgtB"));
        Cli::ControlChannelServer sA, sB;
        sA.setHandler(ackStopHandler());
        sB.setHandler(ackStopHandler());
        QVERIFY(sA.listen(nameA));
        QVERIFY(sB.listen(nameB));
        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(100, nameA), nullptr));
        QVERIFY(Cli::RuntimeStateFile::write(makeEntry(200, nameB), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute(
            {QStringLiteral("--pid"), QStringLiteral("200")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
    }

    void test_pid_not_in_scan_returns_no_running_sim_with_diagnostic()
    {
        using namespace CargoNetSim;

        // One live entry — but user asks for a different PID.
        const QString             name = uniqueName(QStringLiteral("miss"));
        Cli::ControlChannelServer server;
        server.setHandler(ackStopHandler());
        QVERIFY(server.listen(name));
        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(42, name), nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute(
            {QStringLiteral("--pid"), QStringLiteral("9999")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::NoRunningSim));
        QVERIFY(sink.data().contains("9999"));
        QVERIFY(sink.data().contains("not found"));
        QVERIFY(sink.data().contains("42"));  // list of actual pids
    }

    void test_pid_flag_without_value_returns_bad_args()
    {
        using namespace CargoNetSim;

        // Need a non-empty scan so we get past the "no running"
        // early exit; any entry shape works.
        QVERIFY(Cli::RuntimeStateFile::write(
            makeEntry(1, uniqueName(QStringLiteral("v"))),
            nullptr));

        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));
        Cli::StopCommand cmd(&sink);
        const int        rc = cmd.execute({QStringLiteral("--pid")});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::BadArgs));
        QVERIFY(sink.data().contains("--pid requires a value"));
    }
};

QTEST_MAIN(StopCommandTest)
#include "StopCommandTest.moc"
