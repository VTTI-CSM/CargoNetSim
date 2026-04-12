# =============================================================================
# LibrarySearchPaths.cmake
# =============================================================================
# CMake module for setting up library search paths based on build type and platform.
#
# This module provides functions to configure CMAKE_PREFIX_PATH and individual
# library paths to support:
#   1. Build-type-specific installations (e.g., /usr/local/Debug/, /usr/local/Release/)
#   2. Default system installations (e.g., /usr/local/, /usr/)
#   3. Homebrew installations on macOS
#   4. Custom Windows installation paths
#
# Supported Libraries:
#   - Qt6
#   - Container
#   - RabbitMQ-C
#   - KDReports-qt6
#   - Qt6Keychain
#
# Usage:
#   include(LibrarySearchPaths)
#   setup_library_search_paths()
#
# =============================================================================

include_guard(GLOBAL)

# -----------------------------------------------------------------------------
# Function: get_build_type_for_paths
# Returns the build type string for path construction
# For multi-config generators, this will use a generator expression
# -----------------------------------------------------------------------------
function(get_build_type_for_paths OUT_VAR)
    if(CMAKE_BUILD_TYPE)
        set(${OUT_VAR} "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    else()
        # Default to Release if no build type specified
        set(${OUT_VAR} "Release" PARENT_SCOPE)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Function: setup_library_search_paths
# Main function to configure all library search paths
# -----------------------------------------------------------------------------
function(setup_library_search_paths)
    get_build_type_for_paths(CURRENT_BUILD_TYPE)

    message(STATUS "")
    message(STATUS "=== Configuring Library Search Paths ===")
    message(STATUS "Build Type: ${CURRENT_BUILD_TYPE}")
    message(STATUS "")

    # =========================================================================
    # Platform-specific base paths
    # =========================================================================

    if(WIN32)
        _setup_windows_paths("${CURRENT_BUILD_TYPE}")
    elseif(APPLE)
        _setup_macos_paths("${CURRENT_BUILD_TYPE}")
    elseif(UNIX)
        _setup_linux_paths("${CURRENT_BUILD_TYPE}")
    endif()

    message(STATUS "=== Library Search Paths Configured ===")
    message(STATUS "")

    # Propagate CMAKE_PREFIX_PATH to the calling scope
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Internal: Setup Windows library paths
# -----------------------------------------------------------------------------
function(_setup_windows_paths BUILD_TYPE)
    # Build-type-specific paths
    set(WIN_BUILD_TYPE_PREFIX "C:/usr/local/${BUILD_TYPE}")
    set(WIN_DEFAULT_PREFIX "C:/usr/local")

    # Qt6 paths
    set(QT6_SEARCH_PATHS
        "${WIN_BUILD_TYPE_PREFIX}/lib/cmake/Qt6"
        "${WIN_DEFAULT_PREFIX}/lib/cmake/Qt6"
        "C:/Qt/6.8.0/msvc2022_64/lib/cmake/Qt6"
        "C:/Qt/6.7.0/msvc2022_64/lib/cmake/Qt6"
        "$ENV{Qt6_DIR}"
    )
    _find_first_existing_path(QT6_DIR_FOUND "${QT6_SEARCH_PATHS}")
    if(QT6_DIR_FOUND)
        set(Qt6_DIR "${QT6_DIR_FOUND}" CACHE PATH "Qt6 CMake directory" FORCE)
        get_filename_component(QT6_PREFIX "${QT6_DIR_FOUND}/../../.." ABSOLUTE)
        message(STATUS "  Qt6: ${QT6_DIR_FOUND}")
    endif()

    # KDReports paths
    set(KDREPORTS_SEARCH_PATHS
        "${WIN_BUILD_TYPE_PREFIX}/lib/cmake/KDReports-qt6"
        "${WIN_DEFAULT_PREFIX}/lib/cmake/KDReports-qt6"
        "C:/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6"
        "C:/Program Files/KDAB/KDReports/lib/cmake/KDReports-qt6"
        "$ENV{KDREPORTS_DIR}"
    )
    _find_first_existing_path(KDREPORTS_DIR_FOUND "${KDREPORTS_SEARCH_PATHS}")
    if(KDREPORTS_DIR_FOUND)
        set(KDREPORTS_DIR "${KDREPORTS_DIR_FOUND}" CACHE PATH "KDReports CMake directory" FORCE)
        message(STATUS "  KDReports: ${KDREPORTS_DIR_FOUND}")
    else()
        set(KDREPORTS_DIR "C:/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6" CACHE PATH "KDReports CMake directory")
    endif()

    # RabbitMQ-C paths
    set(RABBITMQ_SEARCH_PATHS
        "${WIN_BUILD_TYPE_PREFIX}/lib/cmake/rabbitmq-c"
        "${WIN_DEFAULT_PREFIX}/lib/cmake/rabbitmq-c"
        "C:/Program Files/rabbitmq-c/lib/cmake/rabbitmq-c"
        "C:/Program Files (x86)/rabbitmq-c/lib/cmake/rabbitmq-c"
        "$ENV{RABBITMQ_DIR}"
    )
    _find_first_existing_path(RABBITMQ_CMAKE_DIR_FOUND "${RABBITMQ_SEARCH_PATHS}")
    if(RABBITMQ_CMAKE_DIR_FOUND)
        set(RABBITMQ_CMAKE_DIR "${RABBITMQ_CMAKE_DIR_FOUND}" CACHE PATH "RabbitMQ-C CMake directory" FORCE)
        message(STATUS "  RabbitMQ-C: ${RABBITMQ_CMAKE_DIR_FOUND}")
    else()
        set(RABBITMQ_CMAKE_DIR "C:/Program Files/rabbitmq-c/lib/cmake/rabbitmq-c" CACHE PATH "RabbitMQ-C CMake directory")
    endif()

    # Qt6Keychain paths
    set(QTKEYCHAIN_SEARCH_PATHS
        "${WIN_BUILD_TYPE_PREFIX}/lib/cmake/Qt6Keychain"
        "${WIN_DEFAULT_PREFIX}/lib/cmake/Qt6Keychain"
        "C:/Program Files/qtkeychain/lib/cmake/Qt6Keychain"
        "$ENV{Qt6Keychain_DIR}"
    )
    _find_first_existing_path(QTKEYCHAIN_DIR_FOUND "${QTKEYCHAIN_SEARCH_PATHS}")
    if(QTKEYCHAIN_DIR_FOUND)
        set(Qt6Keychain_DIR "${QTKEYCHAIN_DIR_FOUND}" CACHE PATH "Qt6Keychain CMake directory" FORCE)
        message(STATUS "  Qt6Keychain: ${QTKEYCHAIN_DIR_FOUND}")
    endif()

    # Container library paths
    set(CONTAINER_SEARCH_PATHS
        "${WIN_BUILD_TYPE_PREFIX}/lib/cmake/Container"
        "${WIN_DEFAULT_PREFIX}/lib/cmake/Container"
        "C:/Program Files/ContainerLib/lib/cmake/Container"
        "$ENV{CONTAINER_DIR}"
    )
    _find_first_existing_path(CONTAINER_CMAKE_DIR_FOUND "${CONTAINER_SEARCH_PATHS}")
    if(CONTAINER_CMAKE_DIR_FOUND)
        set(CONTAINER_CMAKE_DIR "${CONTAINER_CMAKE_DIR_FOUND}" CACHE PATH "Container CMake directory" FORCE)
        message(STATUS "  Container: ${CONTAINER_CMAKE_DIR_FOUND}")
    else()
        set(CONTAINER_CMAKE_DIR "C:/Program Files/ContainerLib/lib/cmake/Container" CACHE PATH "Container CMake directory")
    endif()

    # Set CMAKE_PREFIX_PATH
    if(QT6_PREFIX)
        list(PREPEND CMAKE_PREFIX_PATH "${QT6_PREFIX}")
    endif()
    list(APPEND CMAKE_PREFIX_PATH
        "${WIN_BUILD_TYPE_PREFIX}"
        "${WIN_DEFAULT_PREFIX}"
    )
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Internal: Setup macOS library paths
# -----------------------------------------------------------------------------
function(_setup_macos_paths BUILD_TYPE)
    # Homebrew paths (Intel and Apple Silicon)
    set(HOMEBREW_PREFIX_INTEL "/usr/local")
    set(HOMEBREW_PREFIX_ARM "/opt/homebrew")

    # Detect which Homebrew prefix is active
    if(EXISTS "${HOMEBREW_PREFIX_ARM}/bin/brew")
        set(HOMEBREW_PREFIX "${HOMEBREW_PREFIX_ARM}")
    else()
        set(HOMEBREW_PREFIX "${HOMEBREW_PREFIX_INTEL}")
    endif()

    # Build-type-specific prefix
    set(MACOS_BUILD_TYPE_PREFIX "/usr/local/${BUILD_TYPE}")
    set(MACOS_DEFAULT_PREFIX "/usr/local")

    message(STATUS "macOS Homebrew prefix: ${HOMEBREW_PREFIX}")

    # Qt6 paths
    set(QT6_SEARCH_PATHS
        "${MACOS_BUILD_TYPE_PREFIX}/lib/cmake/Qt6"
        "${HOMEBREW_PREFIX}/opt/qt@6/lib/cmake/Qt6"
        "${HOMEBREW_PREFIX}/lib/cmake/Qt6"
        "${MACOS_DEFAULT_PREFIX}/lib/cmake/Qt6"
        "$ENV{HOME}/Qt/6.8.0/macos/lib/cmake/Qt6"
        "$ENV{HOME}/Qt/6.7.0/macos/lib/cmake/Qt6"
        "$ENV{Qt6_DIR}"
    )
    _find_first_existing_path(QT6_DIR_FOUND "${QT6_SEARCH_PATHS}")
    if(QT6_DIR_FOUND)
        set(Qt6_DIR "${QT6_DIR_FOUND}" CACHE PATH "Qt6 CMake directory" FORCE)
        get_filename_component(QT6_PREFIX "${QT6_DIR_FOUND}/../../.." ABSOLUTE)
        message(STATUS "  Qt6: ${QT6_DIR_FOUND}")
    endif()

    # KDReports paths
    set(KDREPORTS_SEARCH_PATHS
        "${MACOS_BUILD_TYPE_PREFIX}/lib/cmake/KDReports-qt6"
        "${MACOS_DEFAULT_PREFIX}/lib/cmake/KDReports-qt6"
        "${HOMEBREW_PREFIX}/lib/cmake/KDReports-qt6"
        "/usr/local/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6"
        "$ENV{KDREPORTS_DIR}"
    )
    _find_first_existing_path(KDREPORTS_DIR_FOUND "${KDREPORTS_SEARCH_PATHS}")
    if(KDREPORTS_DIR_FOUND)
        set(KDREPORTS_DIR "${KDREPORTS_DIR_FOUND}" CACHE PATH "KDReports CMake directory" FORCE)
        message(STATUS "  KDReports: ${KDREPORTS_DIR_FOUND}")
    else()
        set(KDREPORTS_DIR "/usr/local/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6" CACHE PATH "KDReports CMake directory")
    endif()

    # RabbitMQ-C paths
    set(RABBITMQ_SEARCH_PATHS
        "${MACOS_BUILD_TYPE_PREFIX}/lib/cmake/rabbitmq-c"
        "${HOMEBREW_PREFIX}/opt/rabbitmq-c/lib/cmake/rabbitmq-c"
        "${HOMEBREW_PREFIX}/lib/cmake/rabbitmq-c"
        "${MACOS_DEFAULT_PREFIX}/lib/cmake/rabbitmq-c"
        "/usr/local/lib/rabbitmq-c/cmake"
        "$ENV{RABBITMQ_DIR}"
    )
    _find_first_existing_path(RABBITMQ_CMAKE_DIR_FOUND "${RABBITMQ_SEARCH_PATHS}")
    if(RABBITMQ_CMAKE_DIR_FOUND)
        set(RABBITMQ_CMAKE_DIR "${RABBITMQ_CMAKE_DIR_FOUND}" CACHE PATH "RabbitMQ-C CMake directory" FORCE)
        message(STATUS "  RabbitMQ-C: ${RABBITMQ_CMAKE_DIR_FOUND}")
    else()
        set(RABBITMQ_CMAKE_DIR "/usr/local/lib/rabbitmq-c/cmake" CACHE PATH "RabbitMQ-C CMake directory")
    endif()

    # Qt6Keychain paths
    set(QTKEYCHAIN_SEARCH_PATHS
        "${MACOS_BUILD_TYPE_PREFIX}/lib/cmake/Qt6Keychain"
        "${HOMEBREW_PREFIX}/opt/qtkeychain/lib/cmake/Qt6Keychain"
        "${HOMEBREW_PREFIX}/lib/cmake/Qt6Keychain"
        "${MACOS_DEFAULT_PREFIX}/lib/cmake/Qt6Keychain"
        "$ENV{Qt6Keychain_DIR}"
    )
    _find_first_existing_path(QTKEYCHAIN_DIR_FOUND "${QTKEYCHAIN_SEARCH_PATHS}")
    if(QTKEYCHAIN_DIR_FOUND)
        set(Qt6Keychain_DIR "${QTKEYCHAIN_DIR_FOUND}" CACHE PATH "Qt6Keychain CMake directory" FORCE)
        message(STATUS "  Qt6Keychain: ${QTKEYCHAIN_DIR_FOUND}")
    endif()

    # Container library paths
    set(CONTAINER_SEARCH_PATHS
        "${MACOS_BUILD_TYPE_PREFIX}/lib/cmake/Container"
        "${MACOS_DEFAULT_PREFIX}/lib/cmake/Container"
        "${HOMEBREW_PREFIX}/lib/cmake/Container"
        "$ENV{CONTAINER_DIR}"
    )
    _find_first_existing_path(CONTAINER_CMAKE_DIR_FOUND "${CONTAINER_SEARCH_PATHS}")
    if(CONTAINER_CMAKE_DIR_FOUND)
        set(CONTAINER_CMAKE_DIR "${CONTAINER_CMAKE_DIR_FOUND}" CACHE PATH "Container CMake directory" FORCE)
        message(STATUS "  Container: ${CONTAINER_CMAKE_DIR_FOUND}")
    else()
        set(CONTAINER_CMAKE_DIR "/usr/local/lib/cmake/Container" CACHE PATH "Container CMake directory")
    endif()

    # Set CMAKE_PREFIX_PATH
    if(QT6_PREFIX)
        list(PREPEND CMAKE_PREFIX_PATH "${QT6_PREFIX}")
    endif()
    list(APPEND CMAKE_PREFIX_PATH
        "${MACOS_BUILD_TYPE_PREFIX}"
        "${HOMEBREW_PREFIX}"
        "${MACOS_DEFAULT_PREFIX}"
    )
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Internal: Setup Linux library paths
# -----------------------------------------------------------------------------
function(_setup_linux_paths BUILD_TYPE)
    # Build-type-specific prefix
    set(LINUX_BUILD_TYPE_PREFIX "/usr/local/${BUILD_TYPE}")
    set(LINUX_DEFAULT_PREFIX "/usr/local")
    set(LINUX_SYSTEM_PREFIX "/usr")

    message(STATUS "Linux build-type-specific prefix: ${LINUX_BUILD_TYPE_PREFIX}")

    # Qt6 library paths
    set(QT6_SEARCH_PATHS
        "$ENV{HOME}/Qt/6.10.1/gcc_64/lib/cmake/Qt6"
        "$ENV{HOME}/Qt/6.8.0/gcc_64/lib/cmake/Qt6"
        "$ENV{HOME}/Qt/6.7.0/gcc_64/lib/cmake/Qt6"
        "${LINUX_BUILD_TYPE_PREFIX}/lib/cmake/Qt6"
        "${LINUX_DEFAULT_PREFIX}/lib/cmake/Qt6"
        "${LINUX_SYSTEM_PREFIX}/lib/cmake/Qt6"
        "${LINUX_SYSTEM_PREFIX}/lib/x86_64-linux-gnu/cmake/Qt6"
        "$ENV{Qt6_DIR}"
    )
    _find_first_existing_path(QT6_DIR_FOUND "${QT6_SEARCH_PATHS}")
    if(QT6_DIR_FOUND)
        set(Qt6_DIR "${QT6_DIR_FOUND}" CACHE PATH "Qt6 CMake directory" FORCE)
        get_filename_component(QT6_PREFIX "${QT6_DIR_FOUND}/../../.." ABSOLUTE)
        message(STATUS "  Qt6: ${QT6_DIR_FOUND}")
    else()
        set(QT6_PREFIX "")
        message(STATUS "  Qt6: Not found in standard paths, using system search")
    endif()

    # KDReports paths
    set(KDREPORTS_SEARCH_PATHS
        "${LINUX_BUILD_TYPE_PREFIX}/lib/cmake/KDReports-qt6"
        "${LINUX_DEFAULT_PREFIX}/lib/cmake/KDReports-qt6"
        "/usr/local/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6"
        "${LINUX_SYSTEM_PREFIX}/lib/cmake/KDReports-qt6"
        "${LINUX_SYSTEM_PREFIX}/lib/x86_64-linux-gnu/cmake/KDReports-qt6"
        "$ENV{KDREPORTS_DIR}"
    )
    _find_first_existing_path(KDREPORTS_DIR_FOUND "${KDREPORTS_SEARCH_PATHS}")
    if(KDREPORTS_DIR_FOUND)
        set(KDREPORTS_DIR "${KDREPORTS_DIR_FOUND}" CACHE PATH "KDReports CMake directory" FORCE)
        message(STATUS "  KDReports: ${KDREPORTS_DIR_FOUND}")
    else()
        set(KDREPORTS_DIR "/usr/local/KDAB/KDReports-2.3.95/lib/cmake/KDReports-qt6" CACHE PATH "KDReports CMake directory")
        message(STATUS "  KDReports: Using default path (not found)")
    endif()

    # RabbitMQ-C paths
    set(RABBITMQ_SEARCH_PATHS
        "${LINUX_BUILD_TYPE_PREFIX}/lib/cmake/rabbitmq-c"
        "${LINUX_DEFAULT_PREFIX}/lib/cmake/rabbitmq-c"
        "${LINUX_SYSTEM_PREFIX}/lib/cmake/rabbitmq-c"
        "${LINUX_SYSTEM_PREFIX}/lib/x86_64-linux-gnu/cmake/rabbitmq-c"
        "$ENV{RABBITMQ_DIR}"
    )
    _find_first_existing_path(RABBITMQ_CMAKE_DIR_FOUND "${RABBITMQ_SEARCH_PATHS}")
    if(RABBITMQ_CMAKE_DIR_FOUND)
        set(RABBITMQ_CMAKE_DIR "${RABBITMQ_CMAKE_DIR_FOUND}" CACHE PATH "RabbitMQ-C CMake directory" FORCE)
        message(STATUS "  RabbitMQ-C: ${RABBITMQ_CMAKE_DIR_FOUND}")
    else()
        set(RABBITMQ_CMAKE_DIR "/usr/local/lib/cmake/rabbitmq-c" CACHE PATH "RabbitMQ-C CMake directory")
        message(STATUS "  RabbitMQ-C: Using default path (not found)")
    endif()

    # Qt6Keychain paths
    set(QTKEYCHAIN_SEARCH_PATHS
        "${LINUX_BUILD_TYPE_PREFIX}/lib/cmake/Qt6Keychain"
        "${LINUX_DEFAULT_PREFIX}/lib/cmake/Qt6Keychain"
        "${LINUX_SYSTEM_PREFIX}/lib/cmake/Qt6Keychain"
        "${LINUX_SYSTEM_PREFIX}/lib/x86_64-linux-gnu/cmake/Qt6Keychain"
        "$ENV{Qt6Keychain_DIR}"
    )
    _find_first_existing_path(QTKEYCHAIN_DIR_FOUND "${QTKEYCHAIN_SEARCH_PATHS}")
    if(QTKEYCHAIN_DIR_FOUND)
        set(Qt6Keychain_DIR "${QTKEYCHAIN_DIR_FOUND}" CACHE PATH "Qt6Keychain CMake directory" FORCE)
        message(STATUS "  Qt6Keychain: ${QTKEYCHAIN_DIR_FOUND}")
    else()
        message(STATUS "  Qt6Keychain: Using system search paths")
    endif()

    # Container library paths
    set(CONTAINER_SEARCH_PATHS
        "${LINUX_BUILD_TYPE_PREFIX}/lib/cmake/Container"
        "${LINUX_DEFAULT_PREFIX}/lib/cmake/Container"
        "${LINUX_SYSTEM_PREFIX}/lib/cmake/Container"
        "$ENV{CONTAINER_DIR}"
    )
    _find_first_existing_path(CONTAINER_CMAKE_DIR_FOUND "${CONTAINER_SEARCH_PATHS}")
    if(CONTAINER_CMAKE_DIR_FOUND)
        set(CONTAINER_CMAKE_DIR "${CONTAINER_CMAKE_DIR_FOUND}" CACHE PATH "Container CMake directory" FORCE)
        message(STATUS "  Container: ${CONTAINER_CMAKE_DIR_FOUND}")
    else()
        set(CONTAINER_CMAKE_DIR "/usr/local/lib/cmake/Container" CACHE PATH "Container CMake directory")
        message(STATUS "  Container: Using default path")
    endif()

    # Set CMAKE_PREFIX_PATH with Qt6 and build-type-specific paths first
    if(QT6_PREFIX)
        list(PREPEND CMAKE_PREFIX_PATH "${QT6_PREFIX}")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${LINUX_BUILD_TYPE_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH
        "${LINUX_DEFAULT_PREFIX}"
        "${LINUX_SYSTEM_PREFIX}"
    )
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)

    # Propagate variables to parent scope
    set(KDREPORTS_DIR "${KDREPORTS_DIR}" PARENT_SCOPE)
    set(RABBITMQ_CMAKE_DIR "${RABBITMQ_CMAKE_DIR}" PARENT_SCOPE)
    set(Qt6Keychain_DIR "${Qt6Keychain_DIR}" PARENT_SCOPE)
    set(CONTAINER_CMAKE_DIR "${CONTAINER_CMAKE_DIR}" PARENT_SCOPE)
    set(Qt6_DIR "${Qt6_DIR}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Internal: Find first existing path from a list
# -----------------------------------------------------------------------------
function(_find_first_existing_path OUT_VAR SEARCH_PATHS)
    set(${OUT_VAR} "" PARENT_SCOPE)
    foreach(SEARCH_PATH ${SEARCH_PATHS})
        if(EXISTS "${SEARCH_PATH}")
            set(${OUT_VAR} "${SEARCH_PATH}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# Function: get_container_library_paths
# Returns paths to Container library binary and library directories
# Handles build-type-specific naming (d, rd, s suffixes)
# -----------------------------------------------------------------------------
function(get_container_library_paths OUT_BIN_DIR OUT_LIB_DIR)
    get_build_type_for_paths(BUILD_TYPE)

    if(DEFINED CONTAINER_CMAKE_DIR AND EXISTS "${CONTAINER_CMAKE_DIR}")
        # Get parent directories from CMake dir
        get_filename_component(CONTAINER_LIB_CMAKE_DIR "${CONTAINER_CMAKE_DIR}" DIRECTORY)  # lib/cmake
        get_filename_component(CONTAINER_LIB_DIR "${CONTAINER_LIB_CMAKE_DIR}" DIRECTORY)    # lib
        get_filename_component(CONTAINER_PREFIX "${CONTAINER_LIB_DIR}" DIRECTORY)           # prefix

        set(${OUT_LIB_DIR} "${CONTAINER_LIB_DIR}" PARENT_SCOPE)

        if(WIN32)
            set(${OUT_BIN_DIR} "${CONTAINER_PREFIX}/bin" PARENT_SCOPE)
        else()
            set(${OUT_BIN_DIR} "${CONTAINER_LIB_DIR}" PARENT_SCOPE)
        endif()
    else()
        # Fallback to default paths
        if(WIN32)
            set(${OUT_BIN_DIR} "C:/Program Files/ContainerLib/bin" PARENT_SCOPE)
            set(${OUT_LIB_DIR} "C:/Program Files/ContainerLib/lib" PARENT_SCOPE)
        else()
            set(${OUT_BIN_DIR} "/usr/local/lib" PARENT_SCOPE)
            set(${OUT_LIB_DIR} "/usr/local/lib" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Function: get_rabbitmq_library_path
# Returns path to RabbitMQ library directory
# -----------------------------------------------------------------------------
function(get_rabbitmq_library_path OUT_BIN_DIR)
    if(DEFINED RABBITMQ_CMAKE_DIR AND EXISTS "${RABBITMQ_CMAKE_DIR}")
        get_filename_component(RABBITMQ_LIB_CMAKE_DIR "${RABBITMQ_CMAKE_DIR}" DIRECTORY)
        get_filename_component(RABBITMQ_LIB_DIR "${RABBITMQ_LIB_CMAKE_DIR}" DIRECTORY)

        if(WIN32)
            get_filename_component(RABBITMQ_PREFIX "${RABBITMQ_LIB_DIR}" DIRECTORY)
            set(${OUT_BIN_DIR} "${RABBITMQ_PREFIX}/bin" PARENT_SCOPE)
        else()
            set(${OUT_BIN_DIR} "${RABBITMQ_LIB_DIR}" PARENT_SCOPE)
        endif()
    else()
        if(WIN32)
            set(${OUT_BIN_DIR} "C:/Program Files/rabbitmq-c/bin" PARENT_SCOPE)
        else()
            set(${OUT_BIN_DIR} "/usr/local/lib" PARENT_SCOPE)
        endif()
    endif()
endfunction()
