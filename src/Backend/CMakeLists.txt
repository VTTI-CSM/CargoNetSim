# Define the source files for the GUI library
set(Backend_SOURCES

    # Commons
    Commons/LoggerInterface.h
    Commons/ClientType.h
    Commons/TerminalInterface.h
    Commons/TerminalInterface.cpp
    Commons/TransportationMode.h
    Commons/TransportationMode.cpp
    Commons/NetworkKind.h
    Commons/NetworkKind.cpp
    Commons/DirectedGraphBase.h
    Commons/DirectedGraphBase.cpp
    Commons/DirectedGraph.h
    Commons/DirectedGraph.cpp
    Commons/GeoDistance.h
    Commons/GeoDistance.cpp
    Commons/ShortestPathResult.h
    Commons/ThreadSafetyUtils.h
    Commons/ThreadSafetyUtils.cpp
    Commons/LogCategories.h
    Commons/LogCategories.cpp
    Commons/LogMessageHandler.h
    Commons/LogMessageHandler.cpp

    # Models
    Models/TrainSystem.h
    Models/TrainSystem.cpp
    Models/ShipSystem.h
    Models/ShipSystem.cpp
    Models/Terminal.h
    Models/Terminal.cpp
    Models/PathSegment.h
    Models/PathSegment.cpp
    Models/Path.h
    Models/Path.cpp
    Models/BaseObject.h
    Models/BaseObject.cpp
    Models/BaseNetwork.h
    Models/BaseNetwork.cpp
    Models/SimulationTime.h
    Models/SimulationTime.cpp

    # Controllers
    Controllers/VehicleController.h
    Controllers/VehicleController.cpp
    Controllers/NetworkController.h
    Controllers/NetworkController.cpp
    Controllers/RegionDataController.h
    Controllers/RegionDataController.cpp
    Controllers/CargoNetSimController.h
    Controllers/CargoNetSimController.cpp
    Controllers/ConfigController.h
    Controllers/ConfigController.cpp

    # Clients

    # BaseClient
    Clients/BaseClient/RabbitMQHandler.h
    Clients/BaseClient/RabbitMQHandler.cpp
    Clients/BaseClient/SimulationClientBase.h
    Clients/BaseClient/SimulationClientBase.cpp

    # TerminalClient
    Clients/TerminalClient/TerminalSimulationClient.h
    Clients/TerminalClient/TerminalSimulationClient.cpp

    # ShipClient
    Clients/ShipClient/ShipState.h
    Clients/ShipClient/ShipState.cpp
    Clients/ShipClient/SimulationResults.h
    Clients/ShipClient/SimulationResults.cpp
    Clients/ShipClient/SimulationSummaryData.h
    Clients/ShipClient/SimulationSummaryData.cpp
    Clients/ShipClient/ShipSimulationClient.h
    Clients/ShipClient/ShipSimulationClient.cpp

    # TrainClient
    Clients/TrainClient/TrainState.h
    Clients/TrainClient/TrainState.cpp
    Clients/TrainClient/SimulationResults.h
    Clients/TrainClient/SimulationResults.cpp
    Clients/TrainClient/SimulationSummaryData.h
    Clients/TrainClient/SimulationSummaryData.cpp
    Clients/TrainClient/TrainSimulationClient.h
    Clients/TrainClient/TrainSimulationClient.cpp
    Clients/TrainClient/TrainNetwork.h
    Clients/TrainClient/TrainNetwork.cpp


    # TruckClient
    Clients/TruckClient/TruckState.h
    Clients/TruckClient/TruckState.cpp
    Clients/TruckClient/SimulationResults.h
    Clients/TruckClient/SimulationResults.cpp
    Clients/TruckClient/SimulationSummaryData.h
    Clients/TruckClient/SimulationSummaryData.cpp
    Clients/TruckClient/TruckSimulationClient.h
    Clients/TruckClient/TruckSimulationClient.cpp
    Clients/TruckClient/TruckSimulationManager.h
    Clients/TruckClient/TruckSimulationManager.cpp
    Clients/TruckClient/TruckNetwork.h
    Clients/TruckClient/TruckNetwork.cpp
    Clients/TruckClient/AsyncTripManager.h
    Clients/TruckClient/AsyncTripManager.cpp
    Clients/TruckClient/ContainerManager.h
    Clients/TruckClient/ContainerManager.cpp
    Clients/TruckClient/MessageFormatter.h
    Clients/TruckClient/MessageFormatter.cpp
    Clients/TruckClient/SimulationConfig.h
    Clients/TruckClient/SimulationConfig.cpp
    Clients/TruckClient/TripEndCallback.h
    Clients/TruckClient/TripEndCallback.cpp
    Clients/TruckClient/TruckSimulationManager.h
    Clients/TruckClient/TruckSimulationManager.cpp
    Clients/TruckClient/TransportationGraph.h
    Clients/TruckClient/TransportationGraph.cpp
    Clients/TruckClient/IntegrationNode.h
    Clients/TruckClient/IntegrationNode.cpp
    Clients/TruckClient/IntegrationNodeDataReader.h
    Clients/TruckClient/IntegrationNodeDataReader.cpp
    Clients/TruckClient/IntegrationLink.h
    Clients/TruckClient/IntegrationLink.cpp
    Clients/TruckClient/IntegrationLinkDataReader.h
    Clients/TruckClient/IntegrationLinkDataReader.cpp


    Utils/Utils.h


    # Scenario
    Scenario/AutoLinkRule.h
    Scenario/Connection.h
    Scenario/ContainerAllocator.h
    Scenario/ContainerAllocator.cpp
    Scenario/FleetSpec.h
    Scenario/GlobalLink.h
    Scenario/InterfaceConversion.h
    Scenario/InterfaceConversion.cpp
    Scenario/LatLon.h
    Scenario/LinkageSource.h
    Scenario/LinkageStrategy.h
    Scenario/ModeDelayParams.h
    Scenario/NetworkLookup.h
    Scenario/NetworkLookup.cpp
    Scenario/NetworkSpec.h
    Scenario/NodeLinkage.h
    Scenario/OutputSpec.h
    Scenario/PathSimulationResult.h
    Scenario/Point2D.h
    Scenario/PropertyKeys.h
    Scenario/RegionSpec.h
    Scenario/PathAllocation.h
    Scenario/PathDiscovery.h
    Scenario/PathDiscovery.cpp
    Scenario/PathDistancePopulator.h
    Scenario/PathDistancePopulator.cpp
    Scenario/PathKey.h
    Scenario/PathMetrics.h
    Scenario/PathMetricsInputs.h
    Scenario/PathMetricsCalculator.h
    Scenario/PathMetricsCalculator.cpp
    Scenario/ResultsExtractor.h
    Scenario/ResultsExtractor.cpp
    Scenario/ScenarioApplier.h
    Scenario/ScenarioApplier.cpp
    Scenario/ScenarioDocument.h
    Scenario/ScenarioDocument.cpp
    Scenario/ScenarioExecutor.h
    Scenario/ScenarioExecutor.cpp
    Scenario/ScenarioLinker.h
    Scenario/ScenarioLinker.cpp
    Scenario/ScenarioRegistry.h
    Scenario/ScenarioRegistry.cpp
    Scenario/ScenarioRuntime.h
    Scenario/ScenarioRuntime.cpp
    Scenario/ScenarioSerializer.h
    Scenario/ScenarioSerializer.cpp
    Scenario/ScenarioValidator.h
    Scenario/ScenarioValidator.cpp
    Scenario/SegmentCostMath.h
    Scenario/SegmentCostMath.cpp
    Scenario/ServerStatusProbe.h
    Scenario/ServerStatusProbe.cpp
    Scenario/SimulationOrchestrator.h
    Scenario/SimulationOrchestrator.cpp
    Scenario/SimulationRequestBuilder.h
    Scenario/SimulationRequestBuilder.cpp
    Scenario/SimulationSettings.h
    Scenario/SystemDynamicsSpec.h
    Scenario/TerminalCostMath.h
    Scenario/TerminalCostMath.cpp
    Scenario/TerminalPlacement.h
    Scenario/TerminalPlacement.cpp
    Scenario/TerminalTypeDefaults.h
    Scenario/TerminalTypeDefaults.cpp
    Scenario/TruckFleetSpec.h
    Scenario/ValidationIssue.h


    # Initialization
    BackendInit.h
)

# Create the GUI library
add_library(CargoNetSimBackend STATIC ${Backend_SOURCES})

# Link against dependencies
target_link_libraries(CargoNetSimBackend
    PUBLIC
    Qt6::Core
    Qt6::Xml
    Container::Container
    rabbitmq::rabbitmq
    yaml-cpp::yaml-cpp
)

# Conditionally link QtKeychain if available
if(HAVE_QTKEYCHAIN)
    target_link_libraries(CargoNetSimBackend PUBLIC Qt6Keychain::Qt6Keychain)
    target_compile_definitions(CargoNetSimBackend PUBLIC HAVE_QTKEYCHAIN)
endif()

# Set include directories
target_include_directories(CargoNetSimBackend
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}/..
)
