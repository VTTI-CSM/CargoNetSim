#include <QTest>
#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Path.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
    SimulationTime           *timer  = nullptr;
    
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

    template <typename Func>
    auto callOnClientThread(Func &&func)
        -> std::invoke_result_t<std::decay_t<Func>>
    {
        using Result = std::invoke_result_t<std::decay_t<Func>>;

        std::exception_ptr exception;
        QEventLoop         loop;
        QThread           *worker = nullptr;

        if constexpr (std::is_void_v<Result>)
        {
            worker = QThread::create([&]() {
                try
                {
                    func();
                }
                catch (...)
                {
                    exception = std::current_exception();
                }
            });

            QObject::connect(worker, &QThread::finished, &loop,
                             &QEventLoop::quit);
            worker->start();
            loop.exec();
            worker->wait();
            delete worker;

            if (exception)
                std::rethrow_exception(exception);
        }
        else
        {
            std::optional<Result> result;

            worker = QThread::create([&]() {
                try
                {
                    result = func();
                }
                catch (...)
                {
                    exception = std::current_exception();
                }
            });

            QObject::connect(worker, &QThread::finished, &loop,
                             &QEventLoop::quit);
            worker->start();
            loop.exec();
            worker->wait();
            delete worker;

            if (exception)
                std::rethrow_exception(exception);

            return std::move(*result);
        }
    }

private slots:
    /**
     * @brief Set up the test environment before each test
     * 
     * Initializes the client and connects to the server.
     */
    void initTestCase() {
        client = new TerminalSimulationClient();
        timer  = new SimulationTime();

        client->initializeClient(timer);

        QVERIFY(callOnClientThread([this]() {
            return client->connectToServer();
        }));
        QVERIFY(callOnClientThread([this]() {
            return client->resetServer();
        }));
    }

    void init()
    {
        QVERIFY(callOnClientThread([this]() {
            return client->resetServer();
        }));
    }
    
    /**
     * @brief Clean up after each test
     * 
     * Disconnects from the server and cleans up resources.
     */
    void cleanupTestCase() {
        if (client != nullptr)
        {
            callOnClientThread([this]() {
                client->resetServer();
                client->disconnectFromServer();
            });
            delete client;
            client = nullptr;
        }

        delete timer;
        timer = nullptr;
    }
    
    /**
     * @brief Test terminal management operations
     * 
     * Tests adding, querying, and removing terminals.
     */
    void testTerminalManagement() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal1(
                createTestTerminal("Terminal1"));
            return client->addTerminal(terminal1.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal2(
                createTestTerminal("Terminal2"));
            return client->addTerminal(terminal2.get());
        }));

        QCOMPARE(callOnClientThread([this]() {
            return client->getTerminalCount();
        }), 2);

        QVERIFY(callOnClientThread([this]() {
            return client->addTerminalAlias("Terminal1",
                                            "T1Alias");
        }));

        QStringList aliases = callOnClientThread([this]() {
            return client->getTerminalAliases("Terminal1");
        });
        QVERIFY(aliases.contains("T1Alias"));

        const QString canonicalName =
            callOnClientThread([this]() {
                std::unique_ptr<Terminal> status(
                    client->getTerminalStatus("Terminal1"));
                return status ? status->getCanonicalName()
                              : QString();
            });
        QVERIFY(!canonicalName.isEmpty());
        QCOMPARE(canonicalName, QString("Terminal1"));

        QVERIFY(callOnClientThread([this]() {
            return client->removeTerminal("Terminal2");
        }));
        QCOMPARE(callOnClientThread([this]() {
            return client->getTerminalCount();
        }), 1);
    }
    
    /**
     * @brief Test route management operations
     * 
     * Tests adding routes and changing route weights.
     */
    void testRouteManagement() {
        QJsonObject newAttributes;
        newAttributes["distance"] = 150.0;
        newAttributes["cost"] = 75.0;

        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal1(
                createTestTerminal("Terminal1"));
            return client->addTerminal(terminal1.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal2(
                createTestTerminal("Terminal2"));
            return client->addTerminal(terminal2.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<PathSegment> route(
                createTestPathSegment(
                    "Route1", "Terminal1", "Terminal2",
                    CargoNetSim::Backend::TransportationTypes::
                        TransportationMode::Train));
            return client->addRoute(route.get());
        }));
        QVERIFY(callOnClientThread([this, newAttributes]() {
            return client->changeRouteWeight(
                "Terminal1", "Terminal2", 1,
                newAttributes);
        }));

        struct PathSnapshot
        {
            int     size = 0;
            QString start;
            QString end;
        };

        const PathSnapshot path = callOnClientThread([this]() {
            PathSnapshot snapshot;
            QList<PathSegment *> segments =
                client->findShortestPath("Terminal1",
                                         "Terminal2", 1);
            snapshot.size = segments.size();
            if (!segments.isEmpty())
            {
                snapshot.start = segments.first()->getStart();
                snapshot.end   = segments.first()->getEnd();
            }
            qDeleteAll(segments);
            return snapshot;
        });

        QCOMPARE(path.size, 1);
        QCOMPARE(path.start, QString("Terminal1"));
        QCOMPARE(path.end, QString("Terminal2"));
    }
    
    /**
     * @brief Test automatic terminal connection operations
     * 
     * Tests connecting terminals by interface modes.
     */
    void testTerminalConnections() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal1(
                createTestTerminal("Terminal1", "Region1"));
            return client->addTerminal(terminal1.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal2(
                createTestTerminal("Terminal2", "Region1"));
            return client->addTerminal(terminal2.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal3(
                createTestTerminal("Terminal3", "Region2"));
            return client->addTerminal(terminal3.get());
        }));

        QVERIFY(callOnClientThread([this]() {
            return client->connectTerminalsByInterfaceModes();
        }));
        QVERIFY(callOnClientThread([this]() {
            return client->connectTerminalsInRegionByMode(
                "Region1");
        }));
        QVERIFY(callOnClientThread([this]() {
            return client->connectRegionsByMode(1);
        }));

        const bool hasRegionalPath =
            callOnClientThread([this]() {
                QList<PathSegment *> path =
                    client->findShortestPath("Terminal1",
                                             "Terminal2", 1);
                const bool found = !path.isEmpty();
                qDeleteAll(path);
                return found;
            });
        QVERIFY(hasRegionalPath);

        const bool hasCrossRegionPath =
            callOnClientThread([this]() {
                QList<PathSegment *> path =
                    client->findShortestPath("Terminal1",
                                             "Terminal3", 1);
                const bool found = !path.isEmpty();
                qDeleteAll(path);
                return found;
            });
        QVERIFY(hasCrossRegionPath);
    }
    
    /**
     * @brief Test path finding operations
     * 
     * Tests finding shortest paths and multiple paths.
     */
    void testPathFinding() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminalA(
                createTestTerminal("TerminalA"));
            return client->addTerminal(terminalA.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminalB(
                createTestTerminal("TerminalB"));
            return client->addTerminal(terminalB.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminalC(
                createTestTerminal("TerminalC"));
            return client->addTerminal(terminalC.get());
        }));

        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<PathSegment> routeAB(
                createTestPathSegment(
                    "RouteAB", "TerminalA", "TerminalB",
                    CargoNetSim::Backend::TransportationTypes::
                        TransportationMode::Train));
            return client->addRoute(routeAB.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<PathSegment> routeBC(
                createTestPathSegment(
                    "RouteBC", "TerminalB", "TerminalC",
                    CargoNetSim::Backend::TransportationTypes::
                        TransportationMode::Train));
            return client->addRoute(routeBC.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<PathSegment> routeAC(
                createTestPathSegment(
                    "RouteAC", "TerminalA", "TerminalC",
                    CargoNetSim::Backend::TransportationTypes::
                        TransportationMode::Train));
            return client->addRoute(routeAC.get());
        }));

        struct PathSnapshot
        {
            int     size = 0;
            QString start;
            QString end;
        };

        const PathSnapshot shortestPath =
            callOnClientThread([this]() {
                PathSnapshot snapshot;
                QList<PathSegment *> segments =
                    client->findShortestPath("TerminalA",
                                             "TerminalC", 1);
                snapshot.size = segments.size();
                if (!segments.isEmpty())
                {
                    snapshot.start =
                        segments.first()->getStart();
                    snapshot.end = segments.first()->getEnd();
                }
                qDeleteAll(segments);
                return snapshot;
            });
        QCOMPARE(shortestPath.size, 1);
        QCOMPARE(shortestPath.start, QString("TerminalA"));
        QCOMPARE(shortestPath.end, QString("TerminalC"));

        const int topPathCount = callOnClientThread([this]() {
            QList<Path *> topPaths = client->findTopPaths(
                "TerminalA", "TerminalC", 2,
                TransportationTypes::TransportationMode::
                    Train);
            const int count = topPaths.size();
            qDeleteAll(topPaths);
            return count;
        });
        QCOMPARE(topPathCount, 2);
    }
    
    /**
     * @brief Test container management operations
     * 
     * Tests adding, querying, and managing containers.
     */
    void testContainerManagement() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal(
                createTestTerminal("Terminal1"));
            return client->addTerminal(terminal.get());
        }));

        QString containerJson = R"({
            "container_id": "CONT001",
            "weight": 20.0,
            "type": "standard",
            "contents": "test cargo",
            "destination": "DestPort"
        })";

        QVERIFY(callOnClientThread([this, containerJson]() {
            return client->addContainersFromJson(
                "Terminal1", containerJson);
        }));

        QCOMPARE(callOnClientThread([this]() {
            return client->getContainerCount("Terminal1");
        }), 1);

        const double availableCapacity =
            callOnClientThread([this]() {
                return client->getAvailableCapacity("Terminal1");
            });
        QVERIFY(availableCapacity < 1000.0);

        QCOMPARE(callOnClientThread([this]() {
            return client->getMaxCapacity("Terminal1");
        }), 1000.0);

        QVERIFY(callOnClientThread([this]() {
            return client->clearTerminal("Terminal1");
        }));
        QCOMPARE(callOnClientThread([this]() {
            return client->getContainerCount("Terminal1");
        }), 0);
    }
    
    /**
     * @brief Test arrival_mode is passed through addContainers
     *
     * Verifies that the TerminalSimulationClient sends
     * arrival_mode in the RabbitMQ params for all addContainers
     * variants.
     */
    void testAddContainersWithArrivalMode() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal(
                createTestTerminal("ModeTest1"));
            return client->addTerminal(terminal.get());
        }));

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
        bool result = callOnClientThread([this, shipJson]() {
            return client->addContainersFromJson(
                "ModeTest1", shipJson, 1000.0, "Ship");
        });
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
        result = callOnClientThread([this, trainJson]() {
            return client->addContainersFromJson(
                "ModeTest1", trainJson, 2000.0, "Train");
        });
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
        result = callOnClientThread([this, truckJson]() {
            return client->addContainersFromJson(
                "ModeTest1", truckJson, 3000.0, "Truck");
        });
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
        result = callOnClientThread([this, defaultJson]() {
            return client->addContainersFromJson(
                "ModeTest1", defaultJson, 4000.0);
        });
        QVERIFY(result);

        // Verify all 4 containers were added
        QCOMPARE(callOnClientThread([this]() {
            return client->getContainerCount("ModeTest1");
        }), 4);

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
        result = callOnClientThread(
            [this, batchContainers]() {
                QString payload = batchContainers;
                return client->addContainers(
                    "ModeTest1", payload, 5000.0,
                    "Train");
            });
        QVERIFY(result);

        // Verify total: 4 + 2 = 6
        QCOMPARE(callOnClientThread([this]() {
            return client->getContainerCount("ModeTest1");
        }), 6);
    }

    /**
     * @brief Test graph serialization and deserialization
     *
     * Tests the ability to save and restore the network state.
     */
    void testGraphSerialization() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal1(
                createTestTerminal("SerialTest1"));
            return client->addTerminal(terminal1.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal2(
                createTestTerminal("SerialTest2"));
            return client->addTerminal(terminal2.get());
        }));
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<PathSegment> route(
                createTestPathSegment(
                    "SerialRoute", "SerialTest1",
                    "SerialTest2",
                    CargoNetSim::Backend::
                        TransportationTypes::
                            TransportationMode::Train));
            return client->addRoute(route.get());
        }));

        QJsonObject serializedGraph = callOnClientThread([this]() {
            return client->serializeGraph();
        });
        QVERIFY(!serializedGraph.isEmpty());

        QVERIFY(callOnClientThread([this]() {
            return client->resetServer();
        }));
        QCOMPARE(callOnClientThread([this]() {
            return client->getTerminalCount();
        }), 0);

        QVERIFY(callOnClientThread(
            [this, serializedGraph]() {
                return client->deserializeGraph(
                    serializedGraph);
            }));

        QCOMPARE(callOnClientThread([this]() {
            return client->getTerminalCount();
        }), 2);

        const int restoredPathSize =
            callOnClientThread([this]() {
                QList<PathSegment *> path =
                    client->findShortestPath("SerialTest1",
                                             "SerialTest2",
                                             1);
                const int size = path.size();
                qDeleteAll(path);
                return size;
            });
        QCOMPARE(restoredPathSize, 1);
    }
    
    /**
     * @brief Test server reset functionality
     * 
     * Tests that the server can be reset to a clean state.
     */
    void testServerReset() {
        QVERIFY(callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal(
                createTestTerminal("ResetTest"));
            return client->addTerminal(terminal.get());
        }));

        int count = callOnClientThread([this]() {
            return client->getTerminalCount();
        });
        QCOMPARE(count, 1);

        const bool resetSuccess =
            callOnClientThread([this]() {
                return client->resetServer();
            });
        QVERIFY(resetSuccess);

        count = callOnClientThread([this]() {
            return client->getTerminalCount();
        });
        QCOMPARE(count, 0);
    }
    
    /**
     * @brief Test connection robustness
     * 
     * Tests connection robustness through disconnection and reconnection.
     */
    void testConnectionRobustness() {
        bool isConnected = callOnClientThread([this]() {
            return client->isConnected();
        });
        QVERIFY(isConnected);

        callOnClientThread([this]() {
            client->disconnectFromServer();
        });
        QTest::qWait(500);

        isConnected = callOnClientThread([this]() {
            return client->isConnected();
        });
        QVERIFY(!isConnected);

        isConnected = callOnClientThread([this]() {
            return client->connectToServer();
        });
        QVERIFY(isConnected);

        const bool added = callOnClientThread([this]() {
            std::unique_ptr<Terminal> terminal(
                createTestTerminal("ReconnectTest"));
            return client->addTerminal(terminal.get());
        });
        QVERIFY(added);

        const int count = callOnClientThread([this]() {
            return client->getTerminalCount();
        });
        QCOMPARE(count, 1);
    }
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
    QCoreApplication app(argc, argv);

    // Register metatypes only. This test owns a standalone terminal
    // client and must not boot the full controller stack, otherwise a
    // second TerminalSimulationClient competes on the same RabbitMQ
    // response queue.
    const CargoNetSim::Backend::BackendBootstrapService
        backendBootstrap;
    const auto bootstrapResult = backendBootstrap.registerOnly();
    if (!bootstrapResult.succeeded())
    {
        qCritical().noquote()
            << QStringLiteral("TerminalClientTest bootstrap failed: %1")
                   .arg(bootstrapResult.message);
        return 1;
    }

    // Create instance of the test class
    TerminalSimulationClientTest testObject;

    // Run the tests and return the result
    return QTest::qExec(&testObject, argc, argv);
}

#include "TerminalClientTest.moc" 
