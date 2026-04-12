#include <QTest>
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Path.h"
#include "Backend/BackendInit.h"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <memory>
#include <QDebug>

using namespace CargoNetSim::Backend;

/**
 * @class TerminalSimulationClientTest
 * @brief Comprehensive test suite for TerminalSimulationClient class
 * 
 * This test suite validates the functionality of the TerminalSimulationClient,
 * which is responsible for communicating with the TerminalSim server via RabbitMQ.
 * The tests cover terminal management, route operations, path finding, and
 * container operations.
 */
class TerminalSimulationClientTest : public QObject
{
    Q_OBJECT

private:
    // Test objects
    TerminalSimulationClient* client = nullptr;
    SimulationTime           *timer        = nullptr;
    QThread* clientThread = nullptr;
    
    /**
     * @brief Create a test terminal for use in tests
     * 
     * @param name Terminal name
     * @param region Region for the terminal
     * @return Terminal* A new Terminal instance
     */
    Terminal* createTestTerminal(const QString& name, const QString& region = "TestRegion") {
        QStringList names = { name };
        
        // Create a configuration with capacity
        QJsonObject config;
        config["capacity"] = 1000.0;
        config["latitude"] = 42.0;
        config["longitude"] = -71.0;
        
        // Create interfaces for rail and road
        QMap<
            CargoNetSim::Backend::TerminalTypes::
                TerminalInterface,
            QSet<CargoNetSim::Backend::TransportationTypes::
                     TransportationMode>>
            interfaces;
        interfaces[CargoNetSim::Backend::TerminalTypes::
                       TerminalInterface::LAND_SIDE] =
            QSet<CargoNetSim::Backend::TransportationTypes::
                     TransportationMode>()
            << CargoNetSim::Backend::TransportationTypes::
                   TransportationMode::Train; // Rail
        interfaces[CargoNetSim::Backend::TerminalTypes::
                       TerminalInterface::LAND_SIDE] =
            QSet<CargoNetSim::Backend::TransportationTypes::
                     TransportationMode>()
            << CargoNetSim::Backend::TransportationTypes::
                   TransportationMode::Truck; // Truck

        return new Terminal(names, "test", config,
                            interfaces, region);
    }
    
    /**
     * @brief Create a test path segment between terminals
     * 
     * @param segmentId Segment identifier
     * @param start Starting terminal
     * @param end Ending terminal
     * @param mode Transportation mode
     * @return PathSegment* A new PathSegment instance
     */
    PathSegment *createTestPathSegment(
        const QString &segmentId, const QString &start,
        const QString &end,
        CargoNetSim::Backend::TransportationTypes::
            TransportationMode mode)
    {
        // Create attributes with distance and cost
        QJsonObject attributes;
        attributes["distance"] = 100.0;
        attributes["cost"] = 50.0;
        attributes["time"] = 2.0;
        
        return new PathSegment(segmentId, start, end, mode, attributes);
    }

private slots:
    /**
     * @brief Set up the test environment before each test
     * 
     * Initializes the client and connects to the server.
     */
    void initTestCase() {
        // Create a thread for the client first
        clientThread = new QThread();
        clientThread->start();
        
        // Create a client instance with default localhost connection
        // Need to create the client in the correct thread context
        client = nullptr;
        QMetaObject::invokeMethod(clientThread, [this]() {
            client = new TerminalSimulationClient();
            timer  = new SimulationTime();
            client->initializeClient(timer);

            // Emit a signal to continue the test
            QMetaObject::invokeMethod(this, "continueInitialization", Qt::QueuedConnection);

        }, Qt::QueuedConnection);
        
        // Wait for the client to be created
        QSignalSpy spy(this, SIGNAL(initializationCompleted()));
        QVERIFY(spy.wait(5000));
        
        // Ensure connection to the server
        bool connected = false;
        connected      = client->connectToServer();
        QVERIFY(connected);
        
        // Reset server state to ensure clean test environment
        bool reset = client->resetServer();
        QVERIFY(reset);
    }
    
    /**
     * @brief Clean up after each test
     * 
     * Disconnects from the server and cleans up resources.
     */
    void cleanupTestCase() {
        // Reset server state
        QMetaObject::invokeMethod(client, "resetServer", Qt::BlockingQueuedConnection);
        
        // Disconnect from server
        QMetaObject::invokeMethod(client, "disconnectFromServer", Qt::BlockingQueuedConnection);
        
        // Delete the client in its thread context
        QMetaObject::invokeMethod(clientThread, [this]() {
            delete client;
            client = nullptr;
            delete timer;
            timer = nullptr;
        }, Qt::BlockingQueuedConnection);
        
        // Stop and clean up thread
        clientThread->quit();
        clientThread->wait();
        delete clientThread;
        clientThread = nullptr;
    }
    
    /**
     * @brief Test terminal management operations
     * 
     * Tests adding, querying, and removing terminals.
     */
    void testTerminalManagement() {
        // Create test terminals
        std::unique_ptr<Terminal> terminal1(createTestTerminal("Terminal1"));
        std::unique_ptr<Terminal> terminal2(createTestTerminal("Terminal2"));
        
        // Test adding terminals
        QVERIFY(client->addTerminal(terminal1.get()));
        QVERIFY(client->addTerminal(terminal2.get()));
        
        // Test terminal count
        QCOMPARE(client->getTerminalCount(), 2);
        
        // Test adding alias
        QVERIFY(client->addTerminalAlias("Terminal1", "T1Alias"));
        
        // Test getting aliases
        QStringList aliases = client->getTerminalAliases("Terminal1");
        QVERIFY(aliases.contains("T1Alias"));
        
        // Get terminal status
        std::unique_ptr<Terminal> status(client->getTerminalStatus("Terminal1"));
        QVERIFY(status.get() != nullptr);
        QCOMPARE(status->getCanonicalName(), QString("Terminal1"));
        
        // Test removing terminal
        QVERIFY(client->removeTerminal("Terminal2"));
        QCOMPARE(client->getTerminalCount(), 1);
    }
    
    /**
     * @brief Test route management operations
     * 
     * Tests adding routes and changing route weights.
     */
    void testRouteManagement() {
        // Create and add test terminals
        std::unique_ptr<Terminal> terminal1(createTestTerminal("Terminal1"));
        std::unique_ptr<Terminal> terminal2(createTestTerminal("Terminal2"));
        
        QVERIFY(client->addTerminal(terminal1.get()));
        QVERIFY(client->addTerminal(terminal2.get()));
        
        // Create and add a test route
        std::unique_ptr<PathSegment> route(
            createTestPathSegment(
                "Route1", "Terminal1", "Terminal2",
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train));

        QVERIFY(client->addRoute(route.get()));

        // Test changing route weight
        QJsonObject newAttributes;
        newAttributes["distance"] = 150.0;
        newAttributes["cost"] = 75.0;
        
        QVERIFY(client->changeRouteWeight("Terminal1", "Terminal2", 1, newAttributes));
        
        // Find shortest path to verify route was added
        QList<PathSegment*> path = client->findShortestPath("Terminal1", "Terminal2", 1);
        QCOMPARE(path.size(), 1);
        QCOMPARE(path[0]->getStart(), QString("Terminal1"));
        QCOMPARE(path[0]->getEnd(), QString("Terminal2"));
        
        // Clean up path segments (client doesn't own them)
        qDeleteAll(path);
    }
    
    /**
     * @brief Test automatic terminal connection operations
     * 
     * Tests connecting terminals by interface modes.
     */
    void testTerminalConnections() {
        // Create terminals in the same region
        std::unique_ptr<Terminal> terminal1(createTestTerminal("Terminal1", "Region1"));
        std::unique_ptr<Terminal> terminal2(createTestTerminal("Terminal2", "Region1"));
        std::unique_ptr<Terminal> terminal3(createTestTerminal("Terminal3", "Region2"));
        
        QVERIFY(client->addTerminal(terminal1.get()));
        QVERIFY(client->addTerminal(terminal2.get()));
        QVERIFY(client->addTerminal(terminal3.get()));
        
        // Test connecting terminals by interface modes
        QVERIFY(client->connectTerminalsByInterfaceModes());
        
        // Test connecting terminals in a region
        QVERIFY(client->connectTerminalsInRegionByMode("Region1"));
        
        // Test connecting regions
        QVERIFY(client->connectRegionsByMode(1)); // Rail mode
        
        // Verify connections by finding paths
        QList<PathSegment*> path = client->findShortestPath("Terminal1", "Terminal2", 1);
        QVERIFY(!path.isEmpty());
        qDeleteAll(path);
        
        path = client->findShortestPath("Terminal1", "Terminal3", 1);
        QVERIFY(!path.isEmpty());
        qDeleteAll(path);
    }
    
    /**
     * @brief Test path finding operations
     * 
     * Tests finding shortest paths and multiple paths.
     */
    void testPathFinding() {
        // Create a triangle of terminals
        std::unique_ptr<Terminal> terminalA(createTestTerminal("TerminalA"));
        std::unique_ptr<Terminal> terminalB(createTestTerminal("TerminalB"));
        std::unique_ptr<Terminal> terminalC(createTestTerminal("TerminalC"));
        
        QVERIFY(client->addTerminal(terminalA.get()));
        QVERIFY(client->addTerminal(terminalB.get()));
        QVERIFY(client->addTerminal(terminalC.get()));
        
        // Create routes forming a triangle with different costs
        std::unique_ptr<PathSegment> routeAB(
            createTestPathSegment(
                "RouteAB", "TerminalA", "TerminalB",
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train));

        std::unique_ptr<PathSegment> routeBC(
            createTestPathSegment(
                "RouteBC", "TerminalB", "TerminalC",
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train));

        std::unique_ptr<PathSegment> routeAC(
            createTestPathSegment(
                "RouteAC", "TerminalA", "TerminalC",
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train));

        // Add routes to the server
        QVERIFY(client->addRoute(routeAB.get()));
        QVERIFY(client->addRoute(routeBC.get()));
        QVERIFY(client->addRoute(routeAC.get()));
        
        // Test finding the shortest path
        QList<PathSegment*> shortestPath = client->findShortestPath("TerminalA", "TerminalC", 1);
        QCOMPARE(shortestPath.size(), 1);
        QCOMPARE(shortestPath[0]->getStart(), QString("TerminalA"));
        QCOMPARE(shortestPath[0]->getEnd(), QString("TerminalC"));
        qDeleteAll(shortestPath);
        
        // Test finding multiple paths (direct and indirect)
        QList<Path *> topPaths = client->findTopPaths(
            "TerminalA", "TerminalC", 2,
            TransportationTypes::TransportationMode::Train);
        QCOMPARE(topPaths.size(), 2); // Should find 2 paths (direct and via B)
        qDeleteAll(topPaths);
    }
    
    /**
     * @brief Test container management operations
     * 
     * Tests adding, querying, and managing containers.
     */
    void testContainerManagement() {
        // Create a test terminal
        std::unique_ptr<Terminal> terminal(createTestTerminal("Terminal1"));
        QVERIFY(client->addTerminal(terminal.get()));
        
        // Test adding container via JSON
        QString containerJson = R"({
            "container_id": "CONT001",
            "weight": 20.0,
            "type": "standard",
            "contents": "test cargo",
            "destination": "DestPort"
        })";
        
        QVERIFY(client->addContainersFromJson("Terminal1", containerJson));
        
        // Test container count
        QCOMPARE(client->getContainerCount("Terminal1"), 1);
        
        // Test getting available capacity
        double availableCapacity = client->getAvailableCapacity("Terminal1");
        QVERIFY(availableCapacity < 1000.0); // Should be less than max after adding container
        
        // Test getting maximum capacity
        QCOMPARE(client->getMaxCapacity("Terminal1"), 1000.0);
        
        // Test clearing terminal
        QVERIFY(client->clearTerminal("Terminal1"));
        QCOMPARE(client->getContainerCount("Terminal1"), 0);
    }
    
    /**
     * @brief Test arrival_mode is passed through addContainers
     *
     * Verifies that the TerminalSimulationClient sends
     * arrival_mode in the RabbitMQ params for all addContainers
     * variants.
     */
    void testAddContainersWithArrivalMode() {
        // Create and add a test terminal with SD enabled
        std::unique_ptr<Terminal> terminal(createTestTerminal("ModeTest1"));
        QVERIFY(client->addTerminal(terminal.get()));

        // Test 1: addContainersFromJson with arrival_mode "Ship"
        QString shipJson = R"({
            "containers": [{
                "containerID": "MODE-SHIP-001",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }]
        })";
        bool result = client->addContainersFromJson(
            "ModeTest1", shipJson, 1000.0, "Ship");
        QVERIFY(result);

        // Test 2: addContainersFromJson with arrival_mode "Train"
        QString trainJson = R"({
            "containers": [{
                "containerID": "MODE-TRAIN-001",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }]
        })";
        result = client->addContainersFromJson(
            "ModeTest1", trainJson, 2000.0, "Train");
        QVERIFY(result);

        // Test 3: addContainersFromJson with arrival_mode "Truck"
        QString truckJson = R"({
            "containers": [{
                "containerID": "MODE-TRUCK-001",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }]
        })";
        result = client->addContainersFromJson(
            "ModeTest1", truckJson, 3000.0, "Truck");
        QVERIFY(result);

        // Test 4: addContainersFromJson WITHOUT arrival_mode (default)
        QString defaultJson = R"({
            "containers": [{
                "containerID": "MODE-DEFAULT-001",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }]
        })";
        result = client->addContainersFromJson(
            "ModeTest1", defaultJson, 4000.0);
        QVERIFY(result);

        // Verify all 4 containers were added
        QCOMPARE(client->getContainerCount("ModeTest1"), 4);

        // Test 5: addContainers (string/JSON variant) with arrival_mode
        QString batchContainers = R"({
            "containers": [{
                "containerID": "BATCH-MODE-001",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }, {
                "containerID": "BATCH-MODE-002",
                "containerSize": 0,
                "addedTime": 0.0,
                "containerNextDestinations": ["DestPort"],
                "containerCurrentLocation": "Origin"
            }]
        })";
        result = client->addContainers(
            "ModeTest1", batchContainers, 5000.0, "Train");
        QVERIFY(result);

        // Verify total: 4 + 2 = 6
        QCOMPARE(client->getContainerCount("ModeTest1"), 6);
    }

    /**
     * @brief Test graph serialization and deserialization
     *
     * Tests the ability to save and restore the network state.
     */
    void testGraphSerialization() {
        // Create test terminals and routes
        std::unique_ptr<Terminal> terminal1(createTestTerminal("SerialTest1"));
        std::unique_ptr<Terminal> terminal2(createTestTerminal("SerialTest2"));
        
        QVERIFY(client->addTerminal(terminal1.get()));
        QVERIFY(client->addTerminal(terminal2.get()));

        std::unique_ptr<PathSegment> route(
            createTestPathSegment(
                "SerialRoute", "SerialTest1", "SerialTest2",
                CargoNetSim::Backend::TransportationTypes::
                    TransportationMode::Train));
        QVERIFY(client->addRoute(route.get()));
        
        // Serialize the graph
        QJsonObject serializedGraph = client->serializeGraph();
        QVERIFY(!serializedGraph.isEmpty());
        
        // Reset the server
        QVERIFY(client->resetServer());
        QCOMPARE(client->getTerminalCount(), 0);
        
        // Deserialize the graph back
        QVERIFY(client->deserializeGraph(serializedGraph));
        
        // Verify terminals and routes were restored
        QCOMPARE(client->getTerminalCount(), 2);
        
        // Check path is restored
        QList<PathSegment*> path = client->findShortestPath("SerialTest1", "SerialTest2", 1);
        QCOMPARE(path.size(), 1);
        qDeleteAll(path);
    }
    
    /**
     * @brief Test server reset functionality
     * 
     * Tests that the server can be reset to a clean state.
     */
    void testServerReset() {
        // Add a terminal
        std::unique_ptr<Terminal> terminal(createTestTerminal("ResetTest"));

        // Use direct method call to avoid thread issues
        // QMetaObject::invokeMethod(client, "addTerminal",
        // Qt::BlockingQueuedConnection,
        //                          Q_RETURN_ARG(bool,
        //                          QVariant),
        //                          Q_ARG(Terminal*,
        //                          terminal.get()));

        int count = 0;
        QMetaObject::invokeMethod(client, "getTerminalCount", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(int, count));
        QCOMPARE(count, 1);
        
        // Reset the server
        bool resetSuccess = false;
        QMetaObject::invokeMethod(client, "resetServer", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(bool, resetSuccess));
        QVERIFY(resetSuccess);
        
        // Verify server state is reset
        QMetaObject::invokeMethod(client, "getTerminalCount", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(int, count));
        QCOMPARE(count, 0);
    }
    
    /**
     * @brief Test connection robustness
     * 
     * Tests connection robustness through disconnection and reconnection.
     */
    void testConnectionRobustness() {
        // Start with a connected state
        bool isConnected = false;
        QMetaObject::invokeMethod(client, "isConnected", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(bool, isConnected));
        QVERIFY(isConnected);
        
        // Disconnect from server
        QMetaObject::invokeMethod(client, "disconnectFromServer", Qt::BlockingQueuedConnection);
        
        // Allow time for disconnection to complete
        QTest::qWait(500);
        
        // Verify disconnected state
        QMetaObject::invokeMethod(client, "isConnected", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(bool, isConnected));
        QVERIFY(!isConnected);
        
        // Reconnect to server
        QMetaObject::invokeMethod(client, "connectToServer", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(bool, isConnected));
        QVERIFY(isConnected);
        
        // Verify functionality after reconnection
        std::unique_ptr<Terminal> terminal(createTestTerminal("ReconnectTest"));
        bool added = false;
        QMetaObject::invokeMethod(client, "addTerminal", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(bool, added),
                                 Q_ARG(Terminal*, terminal.get()));
        QVERIFY(added);
        
        int count = 0;
        QMetaObject::invokeMethod(client, "getTerminalCount", Qt::BlockingQueuedConnection,
                                 Q_RETURN_ARG(int, count));
        QCOMPARE(count, 1);
    }

private Q_SLOTS:
    void continueInitialization() {
        emit initializationCompleted();
    }

Q_SIGNALS:
    void initializationCompleted();
};

// Set the main source path for proper test file reporting
#define QTEST_MAIN_SOURCE_PATH "tests/TerminalClientTest.cpp"

/**
 * @brief Custom main function for Qt tests
 * 
 * This provides a explicit entry point for the tests that Qt Creator can recognize.
 * It runs all test methods in the TerminalSimulationClientTest class.
 */
int main(int argc, char *argv[])
{   
    // Initialize backend metatypes for proper signal/slot connections across threads
    CargoNetSim::Backend::initializeBackend();
    
    // Create application instance required for the test
    QCoreApplication app(argc, argv);
    
    // Create instance of the test class
    TerminalSimulationClientTest testObject;
    
    // Run the tests and return the result
    return QTest::qExec(&testObject, argc, argv);
}

#include "TerminalClientTest.moc" 
