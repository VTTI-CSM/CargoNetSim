/**
 * @brief Minimal test for arrival_mode parameter passing.
 *
 * Creates a TerminalSimulationClient, connects to RabbitMQ,
 * and exercises addContainers/addContainersFromJson with
 * arrival_mode. Requires a running TerminalSim server.
 */
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Models/SimulationTime.h"
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <QTimer>

using namespace CargoNetSim::Backend;

static int exitCode = 0;

void fail(const QString &msg) {
    qCritical() << "FAIL:" << msg;
    exitCode = 1;
}

void pass(const QString &msg) {
    qDebug() << "PASS:" << msg;
}

void runTests(TerminalSimulationClient *client) {
    // 1. Reset
    if (!client->resetServer()) { fail("resetServer"); return; }
    pass("resetServer");

    // 2. Add terminal
    QStringList names = {"ArrivalModeTestTerminal"};
    QJsonObject config;
    QJsonObject capacity;
    capacity["max_capacity"] = 100;
    capacity["critical_threshold"] = 0.85;
    config["capacity"] = capacity;

    QJsonObject dwellTime;
    dwellTime["method"] = "normal";
    QJsonObject dwellParams;
    dwellParams["mean"] = 172800;
    dwellParams["std_dev"] = 43200;
    dwellTime["parameters"] = dwellParams;
    config["dwell_time"] = dwellTime;

    QJsonObject sd;
    sd["enabled"] = true;
    sd["critical_utilization"] = 0.7;
    sd["max_service_rate"] = 100.0;
    sd["ship_delay_alpha"] = 0.5;
    sd["ship_delay_beta"] = 2.0;
    sd["truck_delay_alpha"] = 0.3;
    sd["truck_delay_beta"] = 2.5;
    sd["train_delay_alpha"] = 0.8;
    sd["train_delay_beta"] = 3.0;
    config["system_dynamics"] = sd;

    QMap<TerminalTypes::TerminalInterface,
         QSet<TransportationTypes::TransportationMode>> interfaces;
    interfaces[TerminalTypes::TerminalInterface::LAND_SIDE]
        = {TransportationTypes::TransportationMode::Truck,
           TransportationTypes::TransportationMode::Train};
    interfaces[TerminalTypes::TerminalInterface::SEA_SIDE]
        = {TransportationTypes::TransportationMode::Ship};

    Terminal terminal(names, "ArrivalModeTest", config, interfaces);
    if (!client->addTerminal(&terminal)) { fail("addTerminal"); return; }
    pass("addTerminal");

    // 3. addContainersFromJson with arrival_mode = "Ship"
    QString shipJson = R"({"containers":[{"containerID":"SHIP-001","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"}]})";
    if (!client->addContainersFromJson("ArrivalModeTestTerminal", shipJson, 1000.0, "Ship")) {
        fail("addContainersFromJson(Ship)"); return;
    }
    pass("addContainersFromJson(Ship)");

    // 4. addContainersFromJson with arrival_mode = "Train"
    QString trainJson = R"({"containers":[{"containerID":"TRAIN-001","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"}]})";
    if (!client->addContainersFromJson("ArrivalModeTestTerminal", trainJson, 2000.0, "Train")) {
        fail("addContainersFromJson(Train)"); return;
    }
    pass("addContainersFromJson(Train)");

    // 5. addContainersFromJson with arrival_mode = "Truck"
    QString truckJson = R"({"containers":[{"containerID":"TRUCK-001","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"}]})";
    if (!client->addContainersFromJson("ArrivalModeTestTerminal", truckJson, 3000.0, "Truck")) {
        fail("addContainersFromJson(Truck)"); return;
    }
    pass("addContainersFromJson(Truck)");

    // 6. addContainersFromJson WITHOUT arrival_mode (default)
    QString defaultJson = R"({"containers":[{"containerID":"DEFAULT-001","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"}]})";
    if (!client->addContainersFromJson("ArrivalModeTestTerminal", defaultJson, 4000.0)) {
        fail("addContainersFromJson(default)"); return;
    }
    pass("addContainersFromJson(default/no mode)");

    // 7. addContainers (string variant) with arrival_mode = "Train"
    QString batchJson = R"({"containers":[{"containerID":"BATCH-001","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"},{"containerID":"BATCH-002","containerSize":0,"addedTime":0.0,"containerNextDestinations":["DestPort"],"containerCurrentLocation":"Origin"}]})";
    if (!client->addContainers("ArrivalModeTestTerminal", batchJson, 5000.0, "Train")) {
        fail("addContainers(string,Train)"); return;
    }
    pass("addContainers(string,Train)");

    // 8. Verify count
    int count = client->getContainerCount("ArrivalModeTestTerminal");
    if (count != 6) {
        fail(QString("Expected 6 containers, got %1").arg(count)); return;
    }
    pass(QString("containerCount = %1").arg(count));

    // 9. Cleanup
    if (!client->resetServer()) { fail("final resetServer"); return; }
    pass("final resetServer");

    qDebug() << "\n=== ALL TESTS PASSED ===";
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    auto *client = new TerminalSimulationClient(nullptr, "localhost", 5672);
    auto *timer = new SimulationTime();
    client->initializeClient(timer);

    if (!client->connectToServer()) {
        qCritical() << "Cannot connect to RabbitMQ";
        return 1;
    }
    qDebug() << "Connected to RabbitMQ";

    // Run tests in a worker thread so the main event loop stays free
    // for RabbitMQ message processing
    QThread *worker = QThread::create([client]() {
        runTests(client);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [](){ QCoreApplication::exit(exitCode); },
            Qt::QueuedConnection);
    });
    worker->start();

    int result = app.exec();
    worker->wait();

    client->disconnectFromServer();
    delete client;
    delete timer;

    return result;
}
