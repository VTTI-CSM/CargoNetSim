#include <QTest>

#include "Backend/Scenario/SimulationOrchestrator.h"
#include "Backend/Scenario/SimulationRequestBuilder.h"

class SimulationOrchestratorTest : public QObject
{
    Q_OBJECT
private slots:
    void test_empty_bundle_is_noop_and_succeeds()
    {
        CargoNetSim::Backend::Scenario::SimulationRequestBundle bundle;
        CargoNetSim::Backend::Scenario::SimulationOrchestrator
            orch(/*ship=*/nullptr,
                 /*train=*/nullptr,
                 /*truck=*/nullptr);
        QString err;
        QVERIFY2(orch.submit(bundle, &err), qPrintable(err));
    }

    void test_submit_fails_when_train_client_is_null()
    {
        using namespace CargoNetSim::Backend::Scenario;

        SimulationRequestBundle b;
        b.trainData["rail_net_a"].append(TrainSimData{});

        SimulationOrchestrator orch(
            /*ship=*/nullptr, /*train=*/nullptr, /*truck=*/nullptr);
        QString err;
        QVERIFY(!orch.submit(b, &err));
        QVERIFY2(err.contains("Train"), qPrintable(err));
    }

    void test_submit_fails_when_truck_manager_is_null()
    {
        using namespace CargoNetSim::Backend::Scenario;

        SimulationRequestBundle b;
        b.truckData["truck_net_a"].append(TruckSimData{});

        SimulationOrchestrator orch(
            /*ship=*/nullptr, /*train=*/nullptr, /*truck=*/nullptr);
        QString err;
        QVERIFY(!orch.submit(b, &err));
        QVERIFY2(err.contains("Truck"), qPrintable(err));
    }

    void test_submit_fails_when_ship_client_is_null()
    {
        using namespace CargoNetSim::Backend::Scenario;

        SimulationRequestBundle b;
        b.shipData["ship_net_a"].append(ShipSimData{});

        SimulationOrchestrator orch(
            /*ship=*/nullptr, /*train=*/nullptr, /*truck=*/nullptr);
        QString err;
        QVERIFY(!orch.submit(b, &err));
        QVERIFY2(err.contains("Ship"), qPrintable(err));
    }
};

QTEST_MAIN(SimulationOrchestratorTest)
#include "SimulationOrchestratorTest.moc"
