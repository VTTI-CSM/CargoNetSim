#include <QCoreApplication>
#include <QJsonObject>
#include <QTest>

#include "CLI/Ipc/ControlChannel.h"

// Contract lock-tests for ControlChannel (Plan 5 Task 9).
//
// Each test uses a server name suffixed with the running test
// process's PID so concurrent runs of the test binary on the same
// host cannot collide.

class ControlChannelTest : public QObject
{
    Q_OBJECT

private:
    /// PID-scoped server name to keep tests isolated.
    static QString uniqueName(const QString &slug)
    {
        return QStringLiteral("cargonetsim-cc-test-%1-%2")
            .arg(slug)
            .arg(QCoreApplication::applicationPid());
    }

private slots:
    void test_server_responds_to_client_with_handler_output()
    {
        using namespace CargoNetSim;
        const QString name = uniqueName(QStringLiteral("ping"));

        Cli::ControlChannelServer server;
        server.setHandler(
            [](const QJsonObject &req) -> QJsonObject {
                if (req.value(QStringLiteral("op")).toString()
                    == QLatin1String("ping"))
                    return QJsonObject{{QStringLiteral("ok"), true}};
                return QJsonObject{
                    {QStringLiteral("error"),
                     QStringLiteral("unknown op")}};
            });
        QVERIFY(server.listen(name));

        const auto reply = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"), QStringLiteral("ping")}},
            /*timeoutMs=*/500);

        QVERIFY(reply.has_value());
        QCOMPARE(reply->value(QStringLiteral("ok")).toBool(), true);
    }

    void test_server_replies_bad_request_when_no_handler_is_set()
    {
        using namespace CargoNetSim;
        const QString name = uniqueName(QStringLiteral("nohandler"));

        // Listen but never call setHandler — server's m_handler is
        // an empty std::function, the defensive branch must fire.
        Cli::ControlChannelServer server;
        QVERIFY(server.listen(name));

        const auto reply = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"), QStringLiteral("ping")}},
            /*timeoutMs=*/500);

        QVERIFY(reply.has_value());
        QCOMPARE(reply->value(QStringLiteral("error")).toString(),
                 QStringLiteral("bad request"));
    }

    void test_client_returns_nullopt_when_server_is_unreachable()
    {
        using namespace CargoNetSim;
        // No server listening on this name — connectToServer must
        // time out and the wrapper returns nullopt.
        const QString name = uniqueName(QStringLiteral("ghost"));

        const auto reply = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"), QStringLiteral("ping")}},
            /*timeoutMs=*/100);

        QVERIFY(!reply.has_value());
    }

    void test_handler_can_distinguish_multiple_op_codes()
    {
        // Confirms the handler signature passes through arbitrary JSON
        // — the server is a transport, not a router. This guards
        // against any future refactor that conflates the two.
        using namespace CargoNetSim;
        const QString name = uniqueName(QStringLiteral("multiop"));

        Cli::ControlChannelServer server;
        server.setHandler(
            [](const QJsonObject &req) -> QJsonObject {
                const QString op = req.value(QStringLiteral("op"))
                                       .toString();
                if (op == QLatin1String("status"))
                    return QJsonObject{
                        {QStringLiteral("running"), true}};
                if (op == QLatin1String("stop"))
                    return QJsonObject{{QStringLiteral("ok"), true}};
                return QJsonObject{
                    {QStringLiteral("error"),
                     QStringLiteral("unknown op")}};
            });
        QVERIFY(server.listen(name));

        const auto status = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"),
                         QStringLiteral("status")}},
            500);
        QVERIFY(status.has_value());
        QCOMPARE(status->value(QStringLiteral("running")).toBool(),
                 true);

        const auto stop = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"),
                         QStringLiteral("stop")}},
            500);
        QVERIFY(stop.has_value());
        QCOMPARE(stop->value(QStringLiteral("ok")).toBool(), true);

        const auto unknown = Cli::ControlChannelClient::send(
            name,
            QJsonObject{{QStringLiteral("op"),
                         QStringLiteral("frobnicate")}},
            500);
        QVERIFY(unknown.has_value());
        QCOMPARE(unknown->value(QStringLiteral("error")).toString(),
                 QStringLiteral("unknown op"));
    }
};

QTEST_MAIN(ControlChannelTest)
#include "ControlChannelTest.moc"
