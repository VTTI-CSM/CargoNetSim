# =============================================================================
# FindDependencies.cmake
# =============================================================================
# CMake module for finding and configuring CargoNetSim dependencies.
#
# This module:
#   1. Sets up library search paths using LibrarySearchPaths module
#   2. Finds all required packages (Qt6, Container, RabbitMQ-C, KDReports)
#   3. Finds optional packages (Qt6Keychain)
#   4. Validates all targets are available
#   5. Sets up library path variables for runtime deployment
#
# =============================================================================

# Include the library search paths module
include(LibrarySearchPaths)

# Setup all library search paths based on platform and build type
setup_library_search_paths()

# =============================================================================
# Qt6 - Required
# =============================================================================
message(STATUS "")
message(STATUS "=== Finding Required Packages ===")

if(CARGONET_BUILD_TESTS)
    find_package(Qt6 COMPONENTS Core Gui Widgets Network Xml Test REQUIRED)
    message(STATUS "Qt6 found: ${Qt6_VERSION} (with Test component)")
else()
    find_package(Qt6 COMPONENTS Core Gui Widgets Network Xml REQUIRED)
    message(STATUS "Qt6 found: ${Qt6_VERSION}")
endif()

# =============================================================================
# Container Library - Required
# =============================================================================
if(CONTAINER_CMAKE_DIR)
    find_package(Container REQUIRED CONFIG PATHS ${CONTAINER_CMAKE_DIR} NO_DEFAULT_PATH)
else()
    find_package(Container REQUIRED CONFIG)
endif()

if(NOT TARGET Container::Container)
    message(FATAL_ERROR "Container::Container target not available after find_package. "
        "Please check your Container library installation and CONTAINER_CMAKE_DIR setting.")
endif()
message(STATUS "Container found: ${Container_VERSION}")

# =============================================================================
# RabbitMQ-C - Required
# =============================================================================
if(RABBITMQ_CMAKE_DIR)
    find_package(RabbitMQ-C REQUIRED CONFIG PATHS ${RABBITMQ_CMAKE_DIR} NO_DEFAULT_PATH)
else()
    find_package(RabbitMQ-C REQUIRED CONFIG)
endif()

if(NOT RabbitMQ-C_FOUND)
    message(FATAL_ERROR "RabbitMQ-C not found. "
        "Please specify the correct path via RABBITMQ_CMAKE_DIR or install rabbitmq-c.")
endif()
message(STATUS "RabbitMQ-C found")

# =============================================================================
# KDReports - Required
# =============================================================================
if(KDREPORTS_DIR)
    find_package(KDReports-qt6 REQUIRED CONFIG PATHS ${KDREPORTS_DIR} NO_DEFAULT_PATH)
else()
    find_package(KDReports-qt6 REQUIRED CONFIG)
endif()

if(NOT KDReports-qt6_FOUND)
    message(FATAL_ERROR "KDReports-qt6 not found. "
        "Please specify the correct path via KDREPORTS_DIR or install KDReports.")
endif()
message(STATUS "KDReports-qt6 found: ${KDReports-qt6_VERSION}")

# =============================================================================
# Qt6Keychain - Optional (for secure password storage)
# =============================================================================
message(STATUS "")
message(STATUS "=== Finding Optional Packages ===")

if(Qt6Keychain_DIR)
    find_package(Qt6Keychain QUIET CONFIG PATHS ${Qt6Keychain_DIR} NO_DEFAULT_PATH)
else()
    find_package(Qt6Keychain QUIET CONFIG)
endif()

if(Qt6Keychain_FOUND)
    message(STATUS "Qt6Keychain found - passwords will be stored securely in OS keychain")
    set(HAVE_QTKEYCHAIN TRUE)
else()
    message(STATUS "Qt6Keychain not found - passwords will be stored in config file")
    message(STATUS "  Install qt6keychain-dev for secure password storage (optional)")
    set(HAVE_QTKEYCHAIN FALSE)
endif()

# =============================================================================
# Setup Runtime Library Paths
# =============================================================================
message(STATUS "")
message(STATUS "=== Configuring Runtime Library Paths ===")

# Get Container library paths
get_container_library_paths(CONTAINER_BIN_DIR CONTAINER_LIB_DIR)
message(STATUS "Container library dir: ${CONTAINER_LIB_DIR}")

# Get RabbitMQ library paths
get_rabbitmq_library_path(RABBITMQ_BIN_DIR)
message(STATUS "RabbitMQ library dir: ${RABBITMQ_BIN_DIR}")

# Set the shared library directory for runtime deployment
if(WIN32)
    set(RABBITMQ_SHRD_LIB_DIR "${RABBITMQ_BIN_DIR}" CACHE PATH "Path to RabbitMQ-C shared libraries")
else()
    set(RABBITMQ_SHRD_LIB_DIR "${RABBITMQ_BIN_DIR}" CACHE PATH "Path to RabbitMQ-C shared libraries")
endif()

message(STATUS "")
message(STATUS "=== Dependency Configuration Complete ===")
message(STATUS "")
