# Tests directory for CargoNetSim using Qt6 Test framework

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test Network Gui Widgets)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

# Live TerminalSim integration tests. Keep the sources in-repo, but do not
# build them as part of the normal CargoNetSim test suite because they depend
# on an external RabbitMQ + TerminalSim runtime and a shared durable response
# queue.
# add_executable(TerminalClientTest TerminalClientTest.cpp)
# target_include_directories(TerminalClientTest PRIVATE ${TEST_INCLUDE_DIRS})
# target_link_libraries(TerminalClientTest PRIVATE
#     Qt6::Core
#     Qt6::Test
#     Qt6::Network
#     CargoNetSimBackend
# )
# set_target_properties(TerminalClientTest PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
# )
#
# Minimal arrival_mode integration test (no Qt Test framework, just runs and
# exits against a live TerminalSim server).
# add_executable(ArrivalModeTest ArrivalModeTest.cpp)
# target_include_directories(ArrivalModeTest PRIVATE ${TEST_INCLUDE_DIRS})
# target_link_libraries(ArrivalModeTest PRIVATE
#     CargoNetSimBackend
#     Qt6::Core
#     Qt6::Network
# )
# set_target_properties(ArrivalModeTest PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
# )

# Scenario model + serializer + validator unit tests
add_executable(ScenarioTest ScenarioTest.cpp)
target_include_directories(ScenarioTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ScenarioTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy scenario fixtures next to the test binary so QFINDTESTDATA / relative
# path resolution work identically on every platform.
add_custom_command(
    TARGET ScenarioTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Geo distance helper tests (Plan 8.2 Task 1)
add_executable(GeoDistanceTest GeoDistanceTest.cpp)
target_include_directories(GeoDistanceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(GeoDistanceTest PRIVATE Qt6::Core Qt6::Test CargoNetSimBackend)
set_target_properties(GeoDistanceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# PathMetricsCalculator unit tests (pure-function math)
add_executable(PathMetricsCalculatorTest PathMetricsCalculatorTest.cpp)
target_include_directories(PathMetricsCalculatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathMetricsCalculatorTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PathMetricsCalculatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_executable(UnitsContractTest UnitsContractTest.cpp)
target_include_directories(UnitsContractTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(UnitsContractTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(UnitsContractTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_executable(TerminalHandoffResolverTest
    TerminalHandoffResolverTest.cpp)
target_include_directories(TerminalHandoffResolverTest PRIVATE
    ${TEST_INCLUDE_DIRS})
target_link_libraries(TerminalHandoffResolverTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(TerminalHandoffResolverTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_executable(TrainNetworkUnitsTest TrainNetworkUnitsTest.cpp)
target_include_directories(TrainNetworkUnitsTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(TrainNetworkUnitsTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(TrainNetworkUnitsTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET TrainNetworkUnitsTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# PathDistancePopulator unit tests (Plan 8.2 Task 3)
add_executable(PathDistancePopulatorTest PathDistancePopulatorTest.cpp)
target_include_directories(PathDistancePopulatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathDistancePopulatorTest PRIVATE Qt6::Core Qt6::Test CargoNetSimBackend)
set_target_properties(PathDistancePopulatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(PathPreparationServiceTest PathPreparationServiceTest.cpp)
target_include_directories(PathPreparationServiceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathPreparationServiceTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PathPreparationServiceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(PreparedPathApplicationServiceTest PreparedPathApplicationServiceTest.cpp)
target_include_directories(PreparedPathApplicationServiceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PreparedPathApplicationServiceTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PreparedPathApplicationServiceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(PreparedPathEligibilityServiceTest PreparedPathEligibilityServiceTest.cpp)
target_include_directories(PreparedPathEligibilityServiceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PreparedPathEligibilityServiceTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PreparedPathEligibilityServiceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Scenario applier tests
add_executable(ScenarioApplierTest ScenarioApplierTest.cpp)
target_include_directories(ScenarioApplierTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioApplierTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ScenarioApplierTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ScenarioApplierTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Scenario linker tests
add_executable(ScenarioLinkerTest ScenarioLinkerTest.cpp)
target_include_directories(ScenarioLinkerTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioLinkerTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ScenarioLinkerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ScenarioLinkerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# PathDiscovery tests (Plan 5 Task 4). Backend-only — no fixture copy
# (the unit slot uses an empty document; the integration slot is
# gated on CARGONETSIM_INTEGRATION and is a placeholder until Task 20).
add_executable(PathDiscoveryTest PathDiscoveryTest.cpp)
target_include_directories(PathDiscoveryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathDiscoveryTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PathDiscoveryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_executable(MossouriScenarioRegressionTest
    MossouriScenarioRegressionTest.cpp)
target_include_directories(MossouriScenarioRegressionTest PRIVATE
    ${TEST_INCLUDE_DIRS})
target_link_libraries(MossouriScenarioRegressionTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
target_compile_definitions(MossouriScenarioRegressionTest PRIVATE
    CARGONETSIM_CLI_BINARY="$<TARGET_FILE:cargonetsim-cli>"
)
add_dependencies(MossouriScenarioRegressionTest cargonetsim-cli)
set_target_properties(MossouriScenarioRegressionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Controller lifetime tests (Tier 1 refactor — Task 1)
add_executable(ControllerLifetimeTest ControllerLifetimeTest.cpp)
target_include_directories(ControllerLifetimeTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ControllerLifetimeTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ControllerLifetimeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Network view service tests
add_executable(NetworkViewServiceTest NetworkViewServiceTest.cpp)
target_include_directories(NetworkViewServiceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(NetworkViewServiceTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(NetworkViewServiceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Scenario executor tests
add_executable(ScenarioExecutorTest ScenarioExecutorTest.cpp)
target_include_directories(ScenarioExecutorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioExecutorTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ScenarioExecutorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ScenarioExecutorTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Server status probe tests
add_executable(ServerStatusProbeTest ServerStatusProbeTest.cpp)
target_include_directories(ServerStatusProbeTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ServerStatusProbeTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ServerStatusProbeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Simulation request builder tests
add_executable(SimulationRequestBuilderTest SimulationRequestBuilderTest.cpp)
target_include_directories(SimulationRequestBuilderTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(SimulationRequestBuilderTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(SimulationRequestBuilderTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET SimulationRequestBuilderTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Simulation orchestrator tests
add_executable(SimulationOrchestratorTest SimulationOrchestratorTest.cpp)
target_include_directories(SimulationOrchestratorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(SimulationOrchestratorTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(SimulationOrchestratorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Phase 2 pathFound remediation regression tests
add_executable(PathFoundRemediationPhase2Test PathFoundRemediationPhase2Test.cpp)
target_include_directories(PathFoundRemediationPhase2Test PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathFoundRemediationPhase2Test PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(PathFoundRemediationPhase2Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET PathFoundRemediationPhase2Test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Results extractor tests
add_executable(ResultsExtractorTest ResultsExtractorTest.cpp)
target_include_directories(ResultsExtractorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ResultsExtractorTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ResultsExtractorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Scenario runtime tests
add_executable(ScenarioRuntimeTest ScenarioRuntimeTest.cpp)
target_include_directories(ScenarioRuntimeTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioRuntimeTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ScenarioRuntimeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# GUI-level tests (Plan 4)

# GUI/backend architecture boundary guard
add_executable(GuiBackendBoundaryTest GUI/GuiBackendBoundaryTest.cpp)
target_include_directories(GuiBackendBoundaryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(GuiBackendBoundaryTest PRIVATE
    Qt6::Core
    Qt6::Test
)
set_target_properties(GuiBackendBoundaryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Item event binder tests
add_executable(ItemEventBinderTest GUI/ItemEventBinderTest.cpp)
target_include_directories(ItemEventBinderTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ItemEventBinderTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ItemEventBinderTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Transportation mode conversion tests
add_executable(TransportationModeConversionTest GUI/TransportationModeConversionTest.cpp)
target_include_directories(TransportationModeConversionTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(TransportationModeConversionTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    CargoNetSimGUI
)
set_target_properties(TransportationModeConversionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Terminal item factory tests
add_executable(TerminalItemFactoryTest GUI/TerminalItemFactoryTest.cpp)
target_include_directories(TerminalItemFactoryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(TerminalItemFactoryTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(TerminalItemFactoryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Map point factory tests
add_executable(MapPointFactoryTest GUI/MapPointFactoryTest.cpp)
target_include_directories(MapPointFactoryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(MapPointFactoryTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(MapPointFactoryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Region center point factory tests
add_executable(RegionCenterPointFactoryTest GUI/RegionCenterPointFactoryTest.cpp)
target_include_directories(RegionCenterPointFactoryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(RegionCenterPointFactoryTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(RegionCenterPointFactoryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Scenario edit service tests
add_executable(ScenarioEditServiceTest GUI/ScenarioEditServiceTest.cpp)
target_include_directories(ScenarioEditServiceTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ScenarioEditServiceTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ScenarioEditServiceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Scene repopulator tests
add_executable(SceneRepopulatorTest GUI/SceneRepopulatorTest.cpp)
target_include_directories(SceneRepopulatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(SceneRepopulatorTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(SceneRepopulatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# TerminalItem view-mode tests
add_executable(TerminalItemViewTest GUI/TerminalItemViewTest.cpp)
target_include_directories(TerminalItemViewTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(TerminalItemViewTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(TerminalItemViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MapPoint view-mode tests
add_executable(MapPointViewTest GUI/MapPointViewTest.cpp)
target_include_directories(MapPointViewTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(MapPointViewTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(MapPointViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ConnectionLine view-mode tests
add_executable(ConnectionLineViewTest GUI/ConnectionLineViewTest.cpp)
target_include_directories(ConnectionLineViewTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ConnectionLineViewTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ConnectionLineViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# RegionCenterPoint view-mode tests
add_executable(RegionCenterPointViewTest GUI/RegionCenterPointViewTest.cpp)
target_include_directories(RegionCenterPointViewTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(RegionCenterPointViewTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(RegionCenterPointViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# GlobalTerminalItem view-mode tests
add_executable(GlobalTerminalItemViewTest GUI/GlobalTerminalItemViewTest.cpp)
target_include_directories(GlobalTerminalItemViewTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(GlobalTerminalItemViewTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(GlobalTerminalItemViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Connection line factory tests
add_executable(ConnectionLineFactoryTest GUI/ConnectionLineFactoryTest.cpp)
target_include_directories(ConnectionLineFactoryTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ConnectionLineFactoryTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ConnectionLineFactoryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# DestinationListEditor widget tests (Plan 9)
add_executable(DestinationListEditorTest GUI/DestinationListEditorTest.cpp)
target_include_directories(DestinationListEditorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(DestinationListEditorTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    CargoNetSimGUI
)
set_target_properties(DestinationListEditorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ProjectSerializer wrapper tests (Task 18)
add_executable(ProjectSerializerTest GUI/ProjectSerializerTest.cpp)
target_include_directories(ProjectSerializerTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ProjectSerializerTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ProjectSerializerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ProjectSerializerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

add_executable(ShortestPathTableIdentityTest
    GUI/ShortestPathTableIdentityTest.cpp)
target_include_directories(ShortestPathTableIdentityTest PRIVATE
    ${TEST_INCLUDE_DIRS})
target_link_libraries(ShortestPathTableIdentityTest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    CargoNetSimGUI
)
set_target_properties(ShortestPathTableIdentityTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ContainerAllocator tests (Plan 10)
add_executable(ContainerAllocatorTest ContainerAllocatorTest.cpp)
target_include_directories(ContainerAllocatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ContainerAllocatorTest PRIVATE
    Qt6::Core
    Qt6::Test
    CargoNetSimBackend
)
set_target_properties(ContainerAllocatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ContainerAllocatorTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Estimated segment physics tests
add_executable(PathSegmentTest PathSegmentTest.cpp)
target_include_directories(PathSegmentTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(PathSegmentTest PRIVATE Qt6::Core Qt6::Test CargoNetSimBackend)
set_target_properties(PathSegmentTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(SegmentPhysicsEstimatorTest SegmentPhysicsEstimatorTest.cpp)
target_include_directories(SegmentPhysicsEstimatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(SegmentPhysicsEstimatorTest PRIVATE Qt6::Core Qt6::Test CargoNetSimBackend)
set_target_properties(SegmentPhysicsEstimatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(EstimatedPhysicsPopulatorTest EstimatedPhysicsPopulatorTest.cpp)
target_include_directories(EstimatedPhysicsPopulatorTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(EstimatedPhysicsPopulatorTest PRIVATE Qt6::Core Qt6::Test CargoNetSimBackend)
set_target_properties(EstimatedPhysicsPopulatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Plan 5 CLI tests — populated incrementally by Tasks 2-20.
add_subdirectory(CLI)
