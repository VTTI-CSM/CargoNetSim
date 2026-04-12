# Tests directory for CargoNetSim using Qt6 Test framework

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test Network)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

add_executable(TerminalClientTest TerminalClientTest.cpp)

target_include_directories(TerminalClientTest PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(TerminalClientTest PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    CargoNetSimBackend
)

set_target_properties(TerminalClientTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Minimal arrival_mode test (no Qt Test framework, just runs and exits)
add_executable(ArrivalModeTest ArrivalModeTest.cpp)
target_include_directories(ArrivalModeTest PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ArrivalModeTest PRIVATE
    CargoNetSimBackend
    Qt6::Core
    Qt6::Network
)
set_target_properties(ArrivalModeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
