#include <QBuffer>
#include <QByteArray>
#include <QList>
#include <QTest>

#include "Backend/Application/AvailabilityService.h"
#include "CLI/Commands/ConnectionsCommand.h"
#include "CLI/ExitCodes.h"

// Lock-tests for ConnectionsCommand (Plan 5 Task 13).
//
// The `--watch` path exercises timing + event loop + SIGINT handling
// which cannot be unit-tested deterministically without clock and
// signal abstractions — Task 20's end-to-end test covers that path
// with real wall time.
//
// The one-shot path is fully covered here by injecting a poller
// lambda that returns canned statuses; no live RabbitMQ or
// CargoNetSimController is needed.

class ConnectionsCommandTest : public QObject
{
    Q_OBJECT

private:
    using Status =
        CargoNetSim::Backend::Application::BackendAvailabilityStatus;

    /// Convenience factory: build a Status with explicit flags.
    static Status make(const QString &name, bool connected,
                       bool hasConsumers)
    {
        Status s;
        s.server       = name;
        s.clientExists = true;
        s.connected    = connected;
        s.hasConsumers = hasConsumers;
        s.commandAvailable = connected && hasConsumers;
        return s;
    }

    /// Wrap a static list as a poller callable for DI.
    static CargoNetSim::Cli::ConnectionsCommand::PollerFn
    makePoller(QList<Status> statuses)
    {
        return [statuses]() { return statuses; };
    }

private slots:
    void test_all_connected_returns_success_and_writes_one_line_per_server()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        QList<Status> statuses = {
            make(QStringLiteral("ship"),     true, true),
            make(QStringLiteral("train"),    true, true),
            make(QStringLiteral("truck"),    true, true),
            make(QStringLiteral("terminal"), true, true),
        };
        Cli::ConnectionsCommand cmd(makePoller(statuses), &sink);
        const int               rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        QCOMPARE(sink.data().count('\n'), 4);
        QVERIFY(sink.data().contains("ship"));
        QVERIFY(sink.data().contains("terminal"));
    }

    void test_any_disconnected_returns_server_disconnected_exit_code()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        QList<Status> statuses = {
            make(QStringLiteral("ship"),     true,  true),
            make(QStringLiteral("train"),    false, false),  // offline
            make(QStringLiteral("truck"),    true,  true),
            make(QStringLiteral("terminal"), true,  true),
        };
        Cli::ConnectionsCommand cmd(makePoller(statuses), &sink);
        const int               rc = cmd.execute({});

        QCOMPARE(rc,
                 static_cast<int>(Cli::ExitCode::ServerDisconnected));
        QVERIFY(sink.data().contains("disconnected"));
    }

    void test_missing_consumers_returns_server_disconnected_exit_code()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        QList<Status> statuses = {
            make(QStringLiteral("ship"),     true, true),
            make(QStringLiteral("train"),    true, false),
            make(QStringLiteral("truck"),    true, true),
            make(QStringLiteral("terminal"), true, true),
        };
        Cli::ConnectionsCommand cmd(makePoller(statuses), &sink);
        const int               rc = cmd.execute({});

        QCOMPARE(rc,
                 static_cast<int>(Cli::ExitCode::ServerDisconnected));
        QVERIFY(sink.data().contains("consumers=0"));
    }

    void test_empty_poller_result_returns_success_with_no_output()
    {
        // Vacuous-good semantics: if the probe returns an empty list,
        // there is nothing to fail — Success is the natural exit.
        // Locks this as an explicit contract so a future refactor does
        // not silently change it.
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        Cli::ConnectionsCommand cmd(makePoller({}), &sink);
        const int               rc = cmd.execute({});

        QCOMPARE(rc, static_cast<int>(Cli::ExitCode::Success));
        QVERIFY(sink.data().isEmpty());
    }

    void test_line_format_contains_server_name_state_and_consumer_count()
    {
        using namespace CargoNetSim;
        QBuffer sink;
        QVERIFY(sink.open(QIODevice::WriteOnly));

        QList<Status> statuses = {
            make(QStringLiteral("ship"), true,  true),
            make(QStringLiteral("rail"), false, false),
        };
        Cli::ConnectionsCommand cmd(makePoller(statuses), &sink);
        cmd.execute({});

        // Line 1: "ship          connected     consumers=1\n"
        // Line 2: "rail          disconnected  consumers=0\n"
        //
        // Width 12 with two-space gutter means the "consumers="
        // column always starts at byte offset 28 from the line start,
        // but the crucial locked invariants are: the server name
        // appears, the connectivity state word appears, and the
        // consumers token carries the right numeric suffix.
        const QByteArray data = sink.data();
        QVERIFY(data.contains("ship"));
        QVERIFY(data.contains("connected"));
        QVERIFY(data.contains("consumers=1"));
        QVERIFY(data.contains("rail"));
        QVERIFY(data.contains("disconnected"));
        QVERIFY(data.contains("consumers=0"));
    }
};

QTEST_MAIN(ConnectionsCommandTest)
#include "ConnectionsCommandTest.moc"
