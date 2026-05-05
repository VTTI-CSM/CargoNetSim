add_library(CargoNetSimUnits INTERFACE)
add_library(CargoNetSim::Units ALIAS CargoNetSimUnits)

target_include_directories(CargoNetSimUnits
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
