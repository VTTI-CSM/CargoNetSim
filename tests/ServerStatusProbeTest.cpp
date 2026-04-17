#include <QTest>

#include "Backend/Scenario/ServerStatusProbe.h"

class ServerStatusProbeTest : public QObject
{
    Q_OBJECT
private slots:
    void test_default_status_has_four_entries()
    {
        CargoNetSim::Backend::Scenario::ServerStatusProbe probe;
        auto statuses = probe.pollAll();
        QCOMPARE(statuses.size(), 4);
        QStringList servers;
        for (const auto &s : statuses)
            servers << s.server;
        QVERIFY(servers.contains("TerminalSim"));
        QVERIFY(servers.contains("NeTrainSim"));
        QVERIFY(servers.contains("ShipNetSim"));
        QVERIFY(servers.contains("INTEGRATION"));
    }

    void test_poll_without_controller_returns_disconnected_entries()
    {
        // CargoNetSimController is a singleton; if it has not been
        // initialized (the default state in this minimal test binary),
        // getShipClient()/getTrainClient()/getTerminalClient()/getTruckManager()
        // return nullptr. The probe must report clientExists=false for all.
        CargoNetSim::Backend::Scenario::ServerStatusProbe probe;
        auto statuses = probe.pollAll();
        for (const auto &s : statuses)
        {
            QVERIFY2(!s.clientExists,
                     qPrintable(QString("Expected %1 clientExists=false")
                                    .arg(s.server)));
            QCOMPARE(s.connected, false);
            QCOMPARE(s.hasConsumers, false);
        }
    }
};

QTEST_MAIN(ServerStatusProbeTest)
#include "ServerStatusProbeTest.moc"
