cmake_minimum_required(VERSION 3.24)

project(CargoNetSimInstaller VERSION ${PROJECT_VERSION})

# Helper function to filter debug DLLs and ensure release versions exist
function(filter_debug_dlls input_dll_list output_dll_list)
    set(filtered_dlls "")
    foreach(dll_path ${${input_dll_list}})
        get_filename_component(dll_name "${dll_path}" NAME)
        # Check if this is a debug DLL (ends with d.dll or _d.dll)
        if(dll_name MATCHES ".*[_]?d\\.dll$")
            # Extract the base name without the debug suffix
            if(dll_name MATCHES ".*_d\\.dll$")
                # Handle _d.dll case
                string(REGEX REPLACE "_d\\.dll$" ".dll" release_dll_name "${dll_name}")
            else()
                # Handle d.dll case
                string(REGEX REPLACE "d\\.dll$" ".dll" release_dll_name "${dll_name}")
            endif()

            get_filename_component(dll_dir "${dll_path}" DIRECTORY)
            set(release_dll_path "${dll_dir}/${release_dll_name}")
            # Only exclude debug DLL if release version exists
            if(EXISTS "${release_dll_path}")
                message(STATUS "Excluding debug DLL (release version exists): ${dll_name}")
            else()
                message(WARNING "Debug DLL found but no release version: ${dll_name} - including anyway")
                list(APPEND filtered_dlls "${dll_path}")
            endif()
        else()
            # Include non-debug DLLs
            list(APPEND filtered_dlls "${dll_path}")
        endif()
    endforeach()
    set(${output_dll_list} "${filtered_dlls}" PARENT_SCOPE)
endfunction()

# Check if building of installer is enabled
if(CARGONET_BUILD_INSTALLER)

    # Define CargoNetSimInstaller as a custom target
    add_custom_target(CargoNetSimInstaller ALL
        COMMENT "Creating an installer for CargoNetSim."
    )

    # Ensure that CargoNetSim is built first by specifying it as a dependency
    add_dependencies(CargoNetSimInstaller CargoNetSim)

    # Define install paths for the executables and libraries
    install(TARGETS CargoNetSim
        RUNTIME DESTINATION bin
        COMPONENT CargoNetSim_COMP
        LIBRARY DESTINATION lib
        COMPONENT CargoNetSim_COMP
        ARCHIVE DESTINATION lib/static
        COMPONENT CargoNetSim_COMP)

    # ========================================
    # RabbitMQ Server Installation Package (OPTIONAL)
    # ========================================
    set(RABBITMQ_INSTALLER_URL "https://github.com/rabbitmq/rabbitmq-server/releases/download/v3.12.10/rabbitmq-server-3.12.10.exe")
    set(RABBITMQ_INSTALLER_FILE "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq-server-installer.exe")

    # Install necessary files for Windows
    if(WIN32)
        # Get Qt parent directory for plugins and libs
        get_filename_component(QT_PARENT_DIR ${QT6_INSTALL_PREFIX} ABSOLUTE)
        set(QT_BIN_DIR "${QT_PARENT_DIR}/bin")

        # ========================================
        # VALIDATION: Check critical Qt DLLs exist
        # ========================================
        set(REQUIRED_QT_DLLS
            "${QT_BIN_DIR}/Qt6Core.dll"
            "${QT_BIN_DIR}/Qt6Network.dll"
            "${QT_BIN_DIR}/Qt6Xml.dll"
            "${QT_BIN_DIR}/Qt6Gui.dll"
            "${QT_BIN_DIR}/Qt6Widgets.dll"
            "${QT_BIN_DIR}/Qt6PrintSupport.dll"
            "${QT_BIN_DIR}/Qt6Sql.dll"
        )

        foreach(QT_DLL ${REQUIRED_QT_DLLS})
            if(NOT EXISTS "${QT_DLL}")
                message(FATAL_ERROR "CRITICAL: Required Qt DLL not found: ${QT_DLL}")
            endif()
        endforeach()

        # ========================================
        # VALIDATION: Check Qt plugins exist
        # ========================================
        if(NOT EXISTS "${QT_PARENT_DIR}/plugins/platforms/")
            message(FATAL_ERROR "CRITICAL: Qt platforms plugin directory not found: ${QT_PARENT_DIR}/plugins/platforms/")
        endif()

        if(NOT EXISTS "${QT_PARENT_DIR}/plugins/imageformats/")
            message(FATAL_ERROR "CRITICAL: Qt imageformats plugin directory not found: ${QT_PARENT_DIR}/plugins/imageformats/")
        endif()

        # ========================================
        # CORE Qt6 DLL files (ESSENTIAL FOR ALL APPS)
        # ========================================
        filter_debug_dlls(REQUIRED_QT_DLLS FILTERED_QT_DLLS)
        install(
            FILES ${FILTERED_QT_DLLS}
            DESTINATION bin
            COMPONENT CargoNetSim_COMP
        )

        # ========================================
        # CRITICAL: Qt6 PLUGINS (REQUIRED FOR GUI APPS TO START)
        # ========================================
        # Platform plugins (ESSENTIAL)
        install(
            DIRECTORY "${QT_PARENT_DIR}/plugins/platforms/"
            DESTINATION bin/plugins/platforms
            COMPONENT CargoNetSim_COMP
            FILES_MATCHING PATTERN "*.dll"
        )

        # Image format plugins (CRITICAL)
        install(
            DIRECTORY "${QT_PARENT_DIR}/plugins/imageformats/"
            DESTINATION bin/plugins/imageformats
            COMPONENT CargoNetSim_COMP
            FILES_MATCHING PATTERN "*.dll"
        )

        # Optional plugin directories (warn if missing)
        set(OPTIONAL_PLUGIN_DIRS "iconengines" "styles" "tls")
        foreach(PLUGIN_DIR ${OPTIONAL_PLUGIN_DIRS})
            if(EXISTS "${QT_PARENT_DIR}/plugins/${PLUGIN_DIR}/")
                install(
                    DIRECTORY "${QT_PARENT_DIR}/plugins/${PLUGIN_DIR}/"
                    DESTINATION bin/plugins/${PLUGIN_DIR}
                    COMPONENT CargoNetSim_COMP
                    FILES_MATCHING PATTERN "*.dll"
                )
            else()
                message(WARNING "Optional Qt plugin directory not found: ${QT_PARENT_DIR}/plugins/${PLUGIN_DIR}/")
            endif()
        endforeach()

        # ========================================
        # THIRD-PARTY LIBRARY DLLs (from your existing project)
        # ========================================
        
        # Container library DLLs (REQUIRED)
        set(CONTAINER_FOUND FALSE)
        if(DEFINED CONTAINER_BIN_DIR AND EXISTS "${CONTAINER_BIN_DIR}")
            file(GLOB CONTAINER_DLLS
                "${CONTAINER_BIN_DIR}/container*.dll"
                "${CONTAINER_BIN_DIR}/Container*.dll"
            )
            if(CONTAINER_DLLS)
                message(STATUS "Found Container DLLs: ${CONTAINER_DLLS}")
                filter_debug_dlls(CONTAINER_DLLS FILTERED_CONTAINER_DLLS)
                install(
                    FILES ${FILTERED_CONTAINER_DLLS}
                    DESTINATION bin
                    COMPONENT CargoNetSim_COMP
                )
                set(CONTAINER_FOUND TRUE)
            endif()
        endif()

        if(NOT CONTAINER_FOUND)
            message(FATAL_ERROR "CRITICAL: Container DLLs not found! CONTAINER_BIN_DIR=${CONTAINER_BIN_DIR}")
        endif()

        # RabbitMQ DLLs (REQUIRED)
        set(RABBITMQ_FOUND FALSE)
        if(DEFINED RABBITMQ_SHRD_LIB_DIR AND EXISTS "${RABBITMQ_SHRD_LIB_DIR}")
            file(GLOB RABBITMQ_DLLS
                "${RABBITMQ_SHRD_LIB_DIR}/*.dll"
            )
            if(RABBITMQ_DLLS)
                message(STATUS "Found RabbitMQ DLLs: ${RABBITMQ_DLLS}")
                filter_debug_dlls(RABBITMQ_DLLS FILTERED_RABBITMQ_DLLS)
                install(
                    FILES ${FILTERED_RABBITMQ_DLLS}
                    DESTINATION bin
                    COMPONENT CargoNetSim_COMP
                )
                set(RABBITMQ_FOUND TRUE)
            endif()
        endif()

        if(NOT RABBITMQ_FOUND)
            message(FATAL_ERROR "CRITICAL: RabbitMQ DLLs not found! RABBITMQ_SHRD_LIB_DIR=${RABBITMQ_SHRD_LIB_DIR}")
        endif()

        # Download RabbitMQ installer
        if(NOT EXISTS "${RABBITMQ_INSTALLER_FILE}")
            message(STATUS "Downloading RabbitMQ Server installer...")
            file(DOWNLOAD "${RABBITMQ_INSTALLER_URL}" "${RABBITMQ_INSTALLER_FILE}"
                SHOW_PROGRESS
                TIMEOUT 300
                STATUS DOWNLOAD_STATUS)

            list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
            if(DOWNLOAD_ERROR)
                message(WARNING "Failed to download RabbitMQ installer. RabbitMQ Server package will be unavailable.")
                set(RABBITMQ_INSTALLER_FILE "")
            else()
                message(STATUS "Successfully downloaded RabbitMQ installer")
            endif()
        endif()

        # Install RabbitMQ installer if download succeeded
        if(EXISTS "${RABBITMQ_INSTALLER_FILE}")
            install(FILES "${RABBITMQ_INSTALLER_FILE}"
                DESTINATION .
                COMPONENT RabbitMQServer_COMP
                RENAME "rabbitmq-server-installer.exe")
        endif()

        # ========================================
        # Visual C++ Runtime Libraries
        # ========================================
        set(VCREDIST_SEARCH_PATHS
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC"
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Redist/MSVC"
            "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC"
            "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Redist/MSVC"
        )

        set(VCREDIST_FOUND FALSE)
        set(VCREDIST_INSTALLER_PATH "")
        foreach(VCREDIST_BASE ${VCREDIST_SEARCH_PATHS})
            if(EXISTS "${VCREDIST_BASE}")
                file(GLOB VCREDIST_SUBDIRS "${VCREDIST_BASE}/*")
                if(VCREDIST_SUBDIRS)
                    list(GET VCREDIST_SUBDIRS -1 LATEST_VCREDIST)
                    
                    # Look for redistributable installer
                    file(GLOB VCREDIST_INSTALLER "${LATEST_VCREDIST}/vcredist_x64.exe")
                    if(VCREDIST_INSTALLER)
                        set(VCREDIST_INSTALLER_PATH ${VCREDIST_INSTALLER})
                    endif()

                    set(VCREDIST_PATHS
                        "${LATEST_VCREDIST}/x64/Microsoft.VC143.CRT"
                        "${LATEST_VCREDIST}/x64/Microsoft.VC142.CRT"
                        "${LATEST_VCREDIST}/x64/Microsoft.VC141.CRT"
                    )

                    foreach(VCREDIST_PATH ${VCREDIST_PATHS})
                        if(EXISTS "${VCREDIST_PATH}")
                            file(GLOB VCREDIST_DLLS "${VCREDIST_PATH}/*.dll")
                            if(VCREDIST_DLLS)
                                message(STATUS "Found VC++ Redistributable DLLs in: ${VCREDIST_PATH}")
                                filter_debug_dlls(VCREDIST_DLLS FILTERED_VCREDIST_DLLS)
                                install(
                                    FILES ${FILTERED_VCREDIST_DLLS}
                                    DESTINATION bin
                                    COMPONENT CargoNetSim_COMP
                                )
                                set(VCREDIST_FOUND TRUE)
                                break()
                            endif()
                        endif()
                    endforeach()
                endif()
                if(VCREDIST_FOUND)
                    break()
                endif()
            endif()
        endforeach()

        # Install VC++ Redistributable installer if found
        if(VCREDIST_INSTALLER_PATH)
            message(STATUS "Found VC++ Redistributable installer: ${VCREDIST_INSTALLER_PATH}")
            install(FILES ${VCREDIST_INSTALLER_PATH}
                DESTINATION .
                COMPONENT CargoNetSim_COMP)
        endif()

        if(NOT VCREDIST_FOUND)
            message(WARNING "Visual C++ Redistributable DLLs not found. Target machines may need VC++ Redist installed.")
        endif()

        # ========================================
        # OPTIONAL: ICU DLLs
        # ========================================
        file(GLOB ICU_DLLS "${QT_BIN_DIR}/icu*.dll")
        if(ICU_DLLS)
            message(STATUS "Found ICU DLLs: ${ICU_DLLS}")
            filter_debug_dlls(ICU_DLLS FILTERED_ICU_DLLS)
            install(
                FILES ${FILTERED_ICU_DLLS}
                DESTINATION bin
                COMPONENT CargoNetSim_COMP
            )
        else()
            message(STATUS "ICU DLLs not found (this is usually OK)")
        endif()

        # ========================================
        # Create qt.conf file for proper plugin discovery
        # ========================================
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
            "[Paths]\nPlugins = plugins\n")
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
            DESTINATION bin
            COMPONENT CargoNetSim_COMP
        )

        # ========================================
        # Extract and set installer icons
        # ========================================
        set(ICON_SOURCE_PATH "${CMAKE_SOURCE_DIR}/resources/images/favicon.ico")

        if(EXISTS "${ICON_SOURCE_PATH}")
            # Copy icon for installer use
            file(COPY "${ICON_SOURCE_PATH}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
            set(INSTALLER_ICON_PATH "${CMAKE_CURRENT_BINARY_DIR}/favicon.ico")
            message(STATUS "Found installer icon: ${ICON_SOURCE_PATH}")
        else()
            message(WARNING "Icon file not found at: ${ICON_SOURCE_PATH}")
        endif()

        # ========================================
        # USER-CONFIGURABLE DLL SEARCH PATHS
        # ========================================
        # Allow users to specify additional search directories for missing DLLs
        set(ADDITIONAL_DLL_SEARCH_PATHS "" CACHE STRING
            "Additional directories to search for missing DLL files (semicolon-separated list)")

        # Define comprehensive list of missing DLL files with their expected locations
        set(MISSING_DLLS_LIST
            # Anaconda DLLs
            "zlib.dll"
            "archive.dll"
            "libcurl.dll"
            "libxml2.dll"
            "libcrypto-1_1-x64.dll"
            "libssl-1_1-x64.dll"
            "liblzma.dll"
            "libdeflate.dll"
            "liblz4.dll"
            "blosc.dll"
            "zstd.dll"
            "Lerc.dll"
            "iconv.dll"
            "LIBBZ2.dll"
            "charset.dll"
            "libssh2.dll"
            "sqlite3.dll"
            "tiff.dll"
            # Additional graphics/image DLLs
            "freetype.dll"
            "libpng16.dll"
        )

        # Standard search paths for missing DLLs
        set(STANDARD_DLL_SEARCH_PATHS
            # Anaconda paths
            "C:/ProgramData/Anaconda3/Library/bin"
            "C:/ProgramData/Miniconda3/Library/bin"
            "C:/Users/$ENV{USERNAME}/Anaconda3/Library/bin"
            "C:/Users/$ENV{USERNAME}/Miniconda3/Library/bin"
            "$ENV{CONDA_PREFIX}/Library/bin"
            # GEOS paths
            "C:/Program Files/GEOS/bin"
            "C:/Program Files (x86)/GEOS/bin"
            # Windows system paths
            "C:/Windows/System32"
            "C:/Windows/SysWOW64"
            # Visual Studio paths
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC"
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Redist/MSVC"
            "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC"
            "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Redist/MSVC"
        )

        # Combine user-specified and standard search paths
        set(ALL_DLL_SEARCH_PATHS ${ADDITIONAL_DLL_SEARCH_PATHS} ${STANDARD_DLL_SEARCH_PATHS})

        # ========================================
        # COMPREHENSIVE MISSING DLL DETECTION AND INSTALLATION
        # ========================================
        function(find_and_install_missing_dlls)
            set(FOUND_DLLS "")
            set(MISSING_DLLS "")

            message(STATUS "=== Searching for Missing DLL Files ===")

            foreach(DLL_NAME ${MISSING_DLLS_LIST})
                set(DLL_FOUND FALSE)
                set(DLL_PATH "")

                # Search in all specified paths
                foreach(SEARCH_PATH ${ALL_DLL_SEARCH_PATHS})
                    # Handle Visual Studio redist paths specially (they have subdirectories)
                    if(SEARCH_PATH MATCHES ".*VC.*Redist.*MSVC.*")
                        file(GLOB VS_SUBDIRS "${SEARCH_PATH}/*")
                        foreach(VS_SUBDIR ${VS_SUBDIRS})
                            if(IS_DIRECTORY "${VS_SUBDIR}")
                                set(VS_SEARCH_PATHS
                                    "${VS_SUBDIR}/x64/Microsoft.VC143.CRT"
                                    "${VS_SUBDIR}/x64/Microsoft.VC142.CRT"
                                    "${VS_SUBDIR}/x64/Microsoft.VC141.CRT"
                                )
                                foreach(VS_PATH ${VS_SEARCH_PATHS})
                                    if(EXISTS "${VS_PATH}/${DLL_NAME}")
                                        set(DLL_PATH "${VS_PATH}/${DLL_NAME}")
                                        set(DLL_FOUND TRUE)
                                        break()
                                    endif()
                                endforeach()
                                if(DLL_FOUND)
                                    break()
                                endif()
                            endif()
                        endforeach()
                    else()
                        # Standard path search
                        if(EXISTS "${SEARCH_PATH}/${DLL_NAME}")
                            set(DLL_PATH "${SEARCH_PATH}/${DLL_NAME}")
                            set(DLL_FOUND TRUE)
                            break()
                        endif()
                    endif()
                endforeach()

                if(DLL_FOUND)
                    list(APPEND FOUND_DLLS "${DLL_PATH}")
                    message(STATUS "✓ Found: ${DLL_NAME} at ${DLL_PATH}")
                else()
                    list(APPEND MISSING_DLLS "${DLL_NAME}")
                    message(WARNING "✗ Missing: ${DLL_NAME}")
                endif()
            endforeach()

            # Install found DLLs
            if(FOUND_DLLS)
                # FIXED: Calculate length first, then use in message
                list(LENGTH FOUND_DLLS FOUND_DLLS_COUNT)
                message(STATUS "Installing ${FOUND_DLLS_COUNT} DLL files...")
                filter_debug_dlls(FOUND_DLLS FILTERED_FOUND_DLLS)
                install(
                    FILES ${FILTERED_FOUND_DLLS}
                    DESTINATION bin
                    COMPONENT CargoNetSim_COMP
                )
            endif()

            # Report summary
            list(LENGTH FOUND_DLLS FOUND_COUNT)
            list(LENGTH MISSING_DLLS MISSING_COUNT)
            message(STATUS "=== DLL Search Summary ===")
            message(STATUS "Found: ${FOUND_COUNT} DLLs")
            message(STATUS "Missing: ${MISSING_COUNT} DLLs")

            if(MISSING_DLLS)
                message(STATUS "Missing DLLs: ${MISSING_DLLS}")
                message(STATUS "To resolve missing DLLs:")
                message(STATUS "1. Install the required software (GEOS, etc.)")
                message(STATUS "2. Add custom search paths using: -DADDITIONAL_DLL_SEARCH_PATHS=\"path1;path2\"")
                message(STATUS "3. Manually copy missing DLLs to your project's bin directory")
            endif()
            message(STATUS "========================")
        endfunction()

        # Call the function to find and install missing DLLs
        find_and_install_missing_dlls()

    # ========================================
    # macOS-SPECIFIC DEPLOYMENT
    # ========================================
    elseif(APPLE)
        message(STATUS "=== Configuring macOS Installer ===")

        # Get Qt parent directory
        get_filename_component(QT_PARENT_DIR ${QT6_INSTALL_PREFIX} ABSOLUTE)

        # Use qt_generate_deploy_app_script for automatic dependency collection (Qt 6.3+)
        if(QT_VERSION VERSION_GREATER_EQUAL "6.3")
            qt_generate_deploy_app_script(
                TARGET CargoNetSim
                OUTPUT_SCRIPT MACOS_DEPLOY_SCRIPT
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
            # Run deployment script at install time
            install(SCRIPT ${MACOS_DEPLOY_SCRIPT}
                COMPONENT CargoNetSim_COMP
            )
        else()
            message(WARNING "Qt version < 6.3 detected. Manual macOS deployment may be required.")
            message(STATUS "Consider using macdeployqt manually after build.")
        endif()

        # Install the app bundle
        install(TARGETS CargoNetSim
            BUNDLE DESTINATION .
            COMPONENT CargoNetSim_COMP
        )

        # Install macOS icon if it exists
        set(MACOS_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/images/CargoNetSim.icns")
        if(EXISTS "${MACOS_ICON_PATH}")
            install(FILES "${MACOS_ICON_PATH}"
                DESTINATION CargoNetSim.app/Contents/Resources
                COMPONENT CargoNetSim_COMP
            )
            message(STATUS "Found macOS icon: ${MACOS_ICON_PATH}")
        else()
            message(WARNING "macOS icon not found at: ${MACOS_ICON_PATH}")
        endif()

        # Install third-party libraries
        # Container library
        if(DEFINED CONTAINER_LIB_DIR AND EXISTS "${CONTAINER_LIB_DIR}")
            file(GLOB CONTAINER_DYLIBS
                "${CONTAINER_LIB_DIR}/libcontainer*.dylib"
                "${CONTAINER_LIB_DIR}/libContainer*.dylib"
            )
            if(CONTAINER_DYLIBS)
                message(STATUS "Found Container libraries: ${CONTAINER_DYLIBS}")
                install(FILES ${CONTAINER_DYLIBS}
                    DESTINATION CargoNetSim.app/Contents/Frameworks
                    COMPONENT CargoNetSim_COMP
                )
            endif()
        endif()

        # RabbitMQ library
        if(DEFINED RABBITMQ_SHRD_LIB_DIR AND EXISTS "${RABBITMQ_SHRD_LIB_DIR}")
            file(GLOB RABBITMQ_DYLIBS "${RABBITMQ_SHRD_LIB_DIR}/*.dylib")
            if(RABBITMQ_DYLIBS)
                message(STATUS "Found RabbitMQ libraries: ${RABBITMQ_DYLIBS}")
                install(FILES ${RABBITMQ_DYLIBS}
                    DESTINATION CargoNetSim.app/Contents/Frameworks
                    COMPONENT CargoNetSim_COMP
                )
            endif()
        endif()

        # Create qt.conf for macOS bundle
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.macos"
            "[Paths]\nPlugins = PlugIns\n")
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.macos"
            DESTINATION CargoNetSim.app/Contents/Resources
            RENAME qt.conf
            COMPONENT CargoNetSim_COMP
        )

    # ========================================
    # LINUX-SPECIFIC DEPLOYMENT
    # ========================================
    elseif(UNIX)
        message(STATUS "=== Configuring Linux Installer ===")

        # Get Qt parent directory
        get_filename_component(QT_PARENT_DIR ${QT6_INSTALL_PREFIX} ABSOLUTE)
        set(QT_LIB_DIR "${QT_PARENT_DIR}/lib")

        # Use qt_generate_deploy_app_script for automatic dependency collection (Qt 6.3+)
        if(QT_VERSION VERSION_GREATER_EQUAL "6.3")
            qt_generate_deploy_app_script(
                TARGET CargoNetSim
                OUTPUT_SCRIPT LINUX_DEPLOY_SCRIPT
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
            # Run deployment script at install time
            install(SCRIPT ${LINUX_DEPLOY_SCRIPT}
                COMPONENT CargoNetSim_COMP
            )
        else()
            message(WARNING "Qt version < 6.3 detected. Manual Linux deployment may be required.")
            # Fallback: Install Qt libraries manually
            set(REQUIRED_QT_LIBS
                "${QT_LIB_DIR}/libQt6Core.so.6"
                "${QT_LIB_DIR}/libQt6Network.so.6"
                "${QT_LIB_DIR}/libQt6Xml.so.6"
                "${QT_LIB_DIR}/libQt6Gui.so.6"
                "${QT_LIB_DIR}/libQt6Widgets.so.6"
                "${QT_LIB_DIR}/libQt6PrintSupport.so.6"
                "${QT_LIB_DIR}/libQt6Sql.so.6"
                "${QT_LIB_DIR}/libQt6DBus.so.6"
                "${QT_LIB_DIR}/libQt6XcbQpa.so.6"
            )
            foreach(QT_LIB ${REQUIRED_QT_LIBS})
                if(EXISTS "${QT_LIB}")
                    install(FILES "${QT_LIB}"
                        DESTINATION lib
                        COMPONENT CargoNetSim_COMP
                    )
                    # Also install the .so symlinks
                    get_filename_component(LIB_NAME "${QT_LIB}" NAME)
                    string(REGEX REPLACE "\\.so\\.6$" ".so" LIB_SYMLINK "${LIB_NAME}")
                    if(EXISTS "${QT_LIB_DIR}/${LIB_SYMLINK}")
                        install(FILES "${QT_LIB_DIR}/${LIB_SYMLINK}"
                            DESTINATION lib
                            COMPONENT CargoNetSim_COMP
                        )
                    endif()
                endif()
            endforeach()
        endif()

        # Install Qt plugins
        if(EXISTS "${QT_PARENT_DIR}/plugins/platforms/")
            install(DIRECTORY "${QT_PARENT_DIR}/plugins/platforms/"
                DESTINATION lib/plugins/platforms
                COMPONENT CargoNetSim_COMP
                FILES_MATCHING PATTERN "*.so"
            )
        endif()

        if(EXISTS "${QT_PARENT_DIR}/plugins/imageformats/")
            install(DIRECTORY "${QT_PARENT_DIR}/plugins/imageformats/"
                DESTINATION lib/plugins/imageformats
                COMPONENT CargoNetSim_COMP
                FILES_MATCHING PATTERN "*.so"
            )
        endif()

        # Optional plugin directories
        set(OPTIONAL_PLUGIN_DIRS "iconengines" "styles" "tls" "xcbglintegrations" "platforminputcontexts")
        foreach(PLUGIN_DIR ${OPTIONAL_PLUGIN_DIRS})
            if(EXISTS "${QT_PARENT_DIR}/plugins/${PLUGIN_DIR}/")
                install(DIRECTORY "${QT_PARENT_DIR}/plugins/${PLUGIN_DIR}/"
                    DESTINATION lib/plugins/${PLUGIN_DIR}
                    COMPONENT CargoNetSim_COMP
                    FILES_MATCHING PATTERN "*.so"
                )
            endif()
        endforeach()

        # Install Container library
        if(DEFINED CONTAINER_LIB_DIR AND EXISTS "${CONTAINER_LIB_DIR}")
            file(GLOB CONTAINER_LIBS
                "${CONTAINER_LIB_DIR}/libcontainer*.so*"
                "${CONTAINER_LIB_DIR}/libContainer*.so*"
            )
            if(CONTAINER_LIBS)
                message(STATUS "Found Container libraries: ${CONTAINER_LIBS}")
                install(FILES ${CONTAINER_LIBS}
                    DESTINATION lib
                    COMPONENT CargoNetSim_COMP
                )
            endif()
        endif()

        # Install RabbitMQ library
        if(DEFINED RABBITMQ_SHRD_LIB_DIR AND EXISTS "${RABBITMQ_SHRD_LIB_DIR}")
            file(GLOB RABBITMQ_LIBS "${RABBITMQ_SHRD_LIB_DIR}/*.so*")
            if(RABBITMQ_LIBS)
                message(STATUS "Found RabbitMQ libraries: ${RABBITMQ_LIBS}")
                install(FILES ${RABBITMQ_LIBS}
                    DESTINATION lib
                    COMPONENT CargoNetSim_COMP
                )
            endif()
        endif()

        # Create qt.conf for Linux
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.linux"
            "[Paths]\nPrefix = ..\nPlugins = lib/plugins\nLibraries = lib\n")
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.linux"
            DESTINATION bin
            RENAME qt.conf
            COMPONENT CargoNetSim_COMP
        )

        # Install .desktop file
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.desktop")
            install(FILES "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.desktop"
                DESTINATION share/applications
                COMPONENT CargoNetSim_COMP
            )
        endif()

        # Install icons for XDG compliance
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.png")
            install(FILES "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.png"
                DESTINATION share/icons/hicolor/256x256/apps
                COMPONENT CargoNetSim_COMP
            )
            # Also install to pixmaps for broader compatibility
            install(FILES "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.png"
                DESTINATION share/pixmaps
                COMPONENT CargoNetSim_COMP
            )
        endif()

        # Create a wrapper script that sets LD_LIBRARY_PATH
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CargoNetSim.sh"
"#!/bin/bash
# CargoNetSim launcher script
SCRIPT_DIR=\"$(cd \"$(dirname \"\${BASH_SOURCE[0]}\")\" && pwd)\"
export LD_LIBRARY_PATH=\"\${SCRIPT_DIR}/../lib:\${LD_LIBRARY_PATH}\"
export QT_PLUGIN_PATH=\"\${SCRIPT_DIR}/../lib/plugins\"
exec \"\${SCRIPT_DIR}/CargoNetSim\" \"$@\"
")
        install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/CargoNetSim.sh"
            DESTINATION bin
            COMPONENT CargoNetSim_COMP
        )

    endif() # Platform-specific deployment

    # ========================================
    # INSTALL DATA DIRECTORIES
    # ========================================
    # Install config directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/config/")
        install(
            DIRECTORY ${CMAKE_SOURCE_DIR}/config/
            DESTINATION config
            COMPONENT CargoNetSim_COMP
        )
    endif()

    # # Install media directory
    # if(EXISTS "${CMAKE_SOURCE_DIR}/media/")
    #     install(
    #         DIRECTORY ${CMAKE_SOURCE_DIR}/media/
    #         DESTINATION media
    #         COMPONENT CargoNetSim_COMP
    #     )
    # endif()

    # Include CPack module to generate the installer
    set(CPACK_GENERATOR "IFW")
    set(CPACK_PACKAGE_VENDOR "CargoNet Development Team")
    set(CPACK_PACKAGE_DESCRIPTION "An installer for CargoNetSim simulation software.")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_NSIS_MODIFY_PATH ON)

    # ========================================
    # CONFIGURE ADMIN vs NON-ADMIN INSTALLATION (QtIFW 4.10 Compatible)
    # ========================================
    if(WIN32)
        # QtIFW 4.10 Enhanced Configuration
        set(CPACK_IFW_PACKAGE_TITLE "CargoNetSim")
        set(CPACK_IFW_PACKAGE_PUBLISHER "CargoNet Development Team")
        set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 800)
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 600)
        set(CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS ON)
        set(CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH ON)

        # QtIFW 4.10-specific features
        set(CPACK_IFW_PACKAGE_WIZARD_SHOW_PAGE_LIST ON)
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/bin/CargoNetSim.exe")
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM_DESCRIPTION "Launch CargoNetSim now")
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM_ARGUMENTS "")

        # Set installer icons
        if(EXISTS "${INSTALLER_ICON_PATH}")
            set(CPACK_IFW_PACKAGE_ICON "${INSTALLER_ICON_PATH}")
            set(CPACK_IFW_PACKAGE_WINDOW_ICON "${INSTALLER_ICON_PATH}")
            message(STATUS "Set installer icon: ${INSTALLER_ICON_PATH}")
        endif()

        # QtIFW 4.10 Performance Optimizations
        set(CPACK_IFW_PACKAGE_COMPRESSION_LEVEL "3")  # Medium compression
        set(CPACK_IFW_VERBOSE ON)
        set(CPACK_IFW_PACKAGE_REMOVE_TARGET_DIR ON)

        # Package-level configuration for proper updates
        set(CPACK_IFW_PACKAGE_NAME "CargoNetSim")
        set(CPACK_IFW_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
        set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "CargoNetSimMaintenanceTool")

        # Create the UI form for the admin selection page
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/adminselectionform.ui" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
        <ui version=\"4.0\">
         <class>AdminSelectionForm</class>
         <widget class=\"QWidget\" name=\"AdminSelectionForm\">
          <layout class=\"QVBoxLayout\">
           <item>
            <widget class=\"QLabel\">
             <property name=\"text\">
              <string>&lt;h3&gt;Installation Type&lt;/h3&gt;</string>
             </property>
            </widget>
           </item>
           <item>
            <widget class=\"QRadioButton\" name=\"adminRadioButton\">
             <property name=\"text\">
              <string>Install for all users (Recommended - Requires Administrator)</string>
             </property>
             <property name=\"checked\">
              <bool>true</bool>
             </property>
            </widget>
           </item>
           <item>
            <widget class=\"QLabel\">
             <property name=\"text\">
              <string>• Installs to Program Files\\n• Available to all users\\n• Requires administrator privileges</string>
             </property>
             <property name=\"indent\">
              <number>20</number>
             </property>
            </widget>
           </item>
           <item>
            <widget class=\"QRadioButton\" name=\"userRadioButton\">
             <property name=\"text\">
              <string>Install for current user only</string>
             </property>
            </widget>
           </item>
           <item>
            <widget class=\"QLabel\">
             <property name=\"text\">
              <string>• Installs to user profile\\n• Available only to current user\\n• No administrator privileges required</string>
             </property>
             <property name=\"indent\">
              <number>20</number>
             </property>
            </widget>
           </item>
          </layout>
         </widget>
        </ui>")

        # Create control script with QtIFW 4.10 admin detection
        set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs" "
function Controller() {
   installer.setValue('StartMenuDir', 'CargoNetSim');
   installer.setMessageBoxAutomaticAnswer('OverwriteTargetDirectory', QMessageBox.Yes);
   installer.setMessageBoxAutomaticAnswer('stopProcessesForUpdates', QMessageBox.Ignore);

   // Set initial default values
   installer.setValue('InstallationType', 'SystemWide');
   installer.setValue('TargetDir', 'C:/Program Files/CargoNetSim');
}

Controller.prototype.IntroductionPageCallback = function() {
   console.log('DEBUG: IntroductionPageCallback - setting defaults');
   installer.setValue('InstallationType', 'SystemWide');
   console.log('DEBUG: Set initial InstallationType to SystemWide');
}

Controller.prototype.DynamicAdminSelectionFormCallback = function() {
   console.log('DEBUG: DynamicAdminSelectionFormCallback triggered');

   var page = gui.pageWidgetByObjectName('DynamicAdminSelectionForm');
   if (!page) {
       console.log('ERROR: Could not get page widget');
       return;
   }

   var adminButton = page.adminRadioButton;
   var userButton = page.userRadioButton;

   // Set initial state based on current InstallationType
   var currentType = installer.value('InstallationType');
   if (currentType === 'UserOnly' && userButton) {
       userButton.checked = true;
   } else if (adminButton) {
       adminButton.checked = true;
   }

   if (adminButton) {
       adminButton.toggled.connect(function(checked) {
           if (checked) {
               console.log('DEBUG: Admin installation selected');
               installer.setValue('InstallationType', 'SystemWide');
               installer.setValue('TargetDir', 'C:/Program Files/CargoNetSim');
           }
       });
   }

   if (userButton) {
       userButton.toggled.connect(function(checked) {
           if (checked) {
               console.log('DEBUG: User installation selected');
               installer.setValue('InstallationType', 'UserOnly');
               var userPath = installer.value('HomeDir') + '/AppData/Local/CargoNetSim';
               installer.setValue('TargetDir', userPath);
           }
       });
   }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
   console.log('DEBUG: TargetDirectoryPageCallback triggered');

   var installType = installer.value('InstallationType');
   console.log('DEBUG: Current installation type: ' + installType);

   if (installType === 'SystemWide') {
       try {
           var isAdmin = false;
           if (typeof installer.hasAdminRights === 'function') {
               isAdmin = installer.hasAdminRights();
           }

           if (!isAdmin) {
               var result = QMessageBox.question('admin_required', 'Administrator Required',
                   'Administrator privileges are required for system-wide installation.\\n\\n' +
                   'Click OK to restart with administrator privileges, or Cancel to install for current user only.',
                   QMessageBox.Ok | QMessageBox.Cancel, QMessageBox.Ok);

               if (result === QMessageBox.Ok) {
                   if (typeof installer.gainAdminRights === 'function') {
                       if (!installer.gainAdminRights()) {
                           QMessageBox.information('elevation_failed', 'Elevation Failed',
                               'Could not obtain administrator privileges.\\n\\nContinuing with user installation.');
                           installer.setValue('InstallationType', 'UserOnly');
                           var userPath = installer.value('HomeDir') + '/AppData/Local/CargoNetSim';
                           installer.setValue('TargetDir', userPath);

                           var widget = gui.currentPageWidget();
                           if (widget && widget.TargetDirectoryLineEdit) {
                               widget.TargetDirectoryLineEdit.setText(userPath);
                           }
                       }
                   }
               } else {
                   installer.setValue('InstallationType', 'UserOnly');
                   var userPath = installer.value('HomeDir') + '/AppData/Local/CargoNetSim';
                   installer.setValue('TargetDir', userPath);

                   var widget = gui.currentPageWidget();
                   if (widget && widget.TargetDirectoryLineEdit) {
                       widget.TargetDirectoryLineEdit.setText(userPath);
                   }
               }
           }
       } catch (e) {
           console.log('DEBUG: Admin rights check failed: ' + e.message);
       }
   }

   var widget = gui.currentPageWidget();
   if (widget && widget.TargetDirectoryLineEdit) {
       var currentPath = installer.value('TargetDir');
       widget.TargetDirectoryLineEdit.setText(currentPath);
   }
}")

        # Create component script optimized for QtIFW 4.10
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cargonet_component.qs" "
function Component() {
    console.log('DEBUG: Adding admin selection page before target directory');
    installer.addWizardPage(component, 'AdminSelectionForm', QInstaller.TargetDirectory);
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        var installType = installer.value('InstallationType');
        console.log('DEBUG: Creating operations for: ' + installType);

        if (installType === 'SystemWide') {
            // System-wide operations
            component.addOperation('RegisterFileType',
                'cns',
                '@TargetDir@/bin/CargoNetSim.exe \"%1\"',
                'CargoNetSim Simulation File',
                'application/x-cargonet');

            component.addOperation('CreateShortcut',
                '@TargetDir@/bin/CargoNetSim.exe',
                '@AllUsersStartMenuProgramsPath@/CargoNetSim/CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/bin/CargoNetSim.exe',
                'description=CargoNetSim Simulation Software');

            component.addOperation('CreateShortcut',
                '@TargetDir@/bin/CargoNetSim.exe',
                '@AllUsersDesktopPath@/CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/bin/CargoNetSim.exe',
                'description=CargoNetSim Simulation Software');

            // Create uninstaller shortcut in Start Menu
            component.addOperation('CreateShortcut',
                '@TargetDir@/CargoNetSimMaintenanceTool.exe',
                '@AllUsersStartMenuProgramsPath@/CargoNetSim/Uninstall CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/CargoNetSimMaintenanceTool.exe',
                'description=Uninstall CargoNetSim');

            if (installer.fileExists('@TargetDir@/vcredist_x64.exe')) {
                component.addOperation('Execute',
                    '@TargetDir@/vcredist_x64.exe',
                    '/install', '/quiet', '/norestart',
                    'UNDOEXECUTE',
                    '@TargetDir@/vcredist_x64.exe', '/uninstall', '/quiet');
            }
        } else {
            // User-only operations
            try {
                component.addOperation('RegisterFileType',
                    'cns',
                    '@TargetDir@/bin/CargoNetSim.exe \"%1\"',
                    'CargoNetSim Simulation File',
                    'application/x-cargonet',
                    'CurrentUser');
            } catch (e) {
                console.log('Warning: Could not register file type for current user: ' + e.message);
            }

            component.addOperation('CreateShortcut',
                '@TargetDir@/bin/CargoNetSim.exe',
                '@UserStartMenuProgramsPath@/CargoNetSim/CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/bin/CargoNetSim.exe',
                'description=CargoNetSim Simulation Software');

            component.addOperation('CreateShortcut',
                '@TargetDir@/bin/CargoNetSim.exe',
                '@UserDesktopPath@/CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/bin/CargoNetSim.exe',
                'description=CargoNetSim Simulation Software');

            // Create uninstaller shortcut in Start Menu
            component.addOperation('CreateShortcut',
                '@TargetDir@/CargoNetSimMaintenanceTool.exe',
                '@UserStartMenuProgramsPath@/CargoNetSim/Uninstall CargoNetSim.lnk',
                'workingDirectory=@TargetDir@',
                'iconPath=@TargetDir@/CargoNetSimMaintenanceTool.exe',
                'description=Uninstall CargoNetSim');
        }

        // Add Windows registry entry for Add/Remove Programs
        if (systemInfo.kernelType === 'winnt') {
            try {
                if (installType === 'SystemWide') {
                    // System-wide registry entries (requires admin)
                    component.addOperation('GlobalConfig',
                        'HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'DisplayName', 'CargoNetSim');
                    component.addOperation('GlobalConfig',
                        'HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'DisplayVersion', '@Version@');
                    component.addOperation('GlobalConfig',
                        'HKEY_LOCAL_MACHINE\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'Publisher', 'CargoNet Development Team');
                } else {
                    // User-only registry entries (no admin required)
                    component.addOperation('GlobalConfig',
                        'HKEY_CURRENT_USER\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'DisplayName', 'CargoNetSim');
                    component.addOperation('GlobalConfig',
                        'HKEY_CURRENT_USER\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'DisplayVersion', '@Version@');
                    component.addOperation('GlobalConfig',
                        'HKEY_CURRENT_USER\\\\SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\CargoNetSim',
                        'Publisher', 'CargoNet Development Team');
                }
            } catch (e) {
                console.log('Warning: Could not write registry entries: ' + e.message);
            }
        }
    } catch (e) {
        console.log('ERROR in createOperations: ' + e.message);
    }
}")

    # ========================================
    # macOS CPack IFW CONFIGURATION
    # ========================================
    elseif(APPLE)
        set(CPACK_IFW_PACKAGE_TITLE "CargoNetSim")
        set(CPACK_IFW_PACKAGE_PUBLISHER "CargoNet Development Team")
        set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 800)
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 600)
        set(CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS ON)
        set(CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH ON)

        # macOS-specific settings
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/CargoNetSim.app/Contents/MacOS/CargoNetSim")
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM_DESCRIPTION "Launch CargoNetSim now")

        # Set macOS icon if it exists
        set(MACOS_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/images/CargoNetSim.icns")
        if(EXISTS "${MACOS_ICON_PATH}")
            set(CPACK_IFW_PACKAGE_ICON "${MACOS_ICON_PATH}")
            set(CPACK_IFW_PACKAGE_WINDOW_ICON "${MACOS_ICON_PATH}")
            message(STATUS "Set macOS installer icon: ${MACOS_ICON_PATH}")
        endif()

        # QtIFW settings
        set(CPACK_IFW_VERBOSE ON)
        set(CPACK_IFW_PACKAGE_REMOVE_TARGET_DIR ON)
        set(CPACK_IFW_PACKAGE_NAME "CargoNetSim")
        set(CPACK_IFW_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
        set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "CargoNetSimMaintenanceTool")

        # Create macOS control script
        set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs" "
function Controller() {
    installer.setMessageBoxAutomaticAnswer('OverwriteTargetDirectory', QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer('stopProcessesForUpdates', QMessageBox.Ignore);

    // Set default installation directory for macOS
    installer.setValue('TargetDir', '/Applications/CargoNetSim');
}

Controller.prototype.IntroductionPageCallback = function() {
    console.log('DEBUG: macOS Installation - IntroductionPageCallback');
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget && widget.TargetDirectoryLineEdit) {
        var currentPath = installer.value('TargetDir');
        widget.TargetDirectoryLineEdit.setText(currentPath);
    }
}")

        # Create macOS component script
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cargonet_component.qs" "
function Component() {
    // macOS component - no admin selection page needed
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        var targetDir = installer.value('TargetDir');
        console.log('DEBUG: macOS - Creating operations for target: ' + targetDir);

        // Create symlink in /Applications (optional - user may not have permissions)
        // Most users will install directly to /Applications
        if (targetDir !== '/Applications/CargoNetSim') {
            try {
                component.addOperation('Execute', 'ln', '-sf',
                    targetDir + '/CargoNetSim.app',
                    '/Applications/CargoNetSim.app',
                    'UNDOEXECUTE', 'rm', '-f', '/Applications/CargoNetSim.app');
            } catch (e) {
                console.log('Note: Could not create symlink in /Applications: ' + e.message);
            }
        }
    } catch (e) {
        console.log('ERROR in macOS createOperations: ' + e.message);
    }
}")

    # ========================================
    # LINUX CPack IFW CONFIGURATION
    # ========================================
    elseif(UNIX)
        set(CPACK_IFW_PACKAGE_TITLE "CargoNetSim")
        set(CPACK_IFW_PACKAGE_PUBLISHER "CargoNet Development Team")
        set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 800)
        set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 600)
        set(CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS ON)
        set(CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH ON)

        # Linux-specific settings
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/bin/CargoNetSim.sh")
        set(CPACK_IFW_PACKAGE_RUN_PROGRAM_DESCRIPTION "Launch CargoNetSim now")

        # Use PNG icon for Linux
        set(LINUX_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/linux/CargoNetSim.png")
        if(EXISTS "${LINUX_ICON_PATH}")
            # QtIFW on Linux uses PNG icons
            set(CPACK_IFW_PACKAGE_ICON "${LINUX_ICON_PATH}")
            set(CPACK_IFW_PACKAGE_WINDOW_ICON "${LINUX_ICON_PATH}")
            message(STATUS "Set Linux installer icon: ${LINUX_ICON_PATH}")
        endif()

        # QtIFW settings
        set(CPACK_IFW_VERBOSE ON)
        set(CPACK_IFW_PACKAGE_REMOVE_TARGET_DIR ON)
        set(CPACK_IFW_PACKAGE_NAME "CargoNetSim")
        set(CPACK_IFW_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
        set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "CargoNetSimMaintenanceTool")

        # Create Linux control script
        set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/controlscript.qs" "
function Controller() {
    installer.setMessageBoxAutomaticAnswer('OverwriteTargetDirectory', QMessageBox.Yes);
    installer.setMessageBoxAutomaticAnswer('stopProcessesForUpdates', QMessageBox.Ignore);

    // Set default installation directory for Linux
    var homeDir = installer.value('HomeDir');
    installer.setValue('TargetDir', homeDir + '/CargoNetSim');
}

Controller.prototype.IntroductionPageCallback = function() {
    console.log('DEBUG: Linux Installation - IntroductionPageCallback');
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var widget = gui.currentPageWidget();
    if (widget && widget.TargetDirectoryLineEdit) {
        var currentPath = installer.value('TargetDir');
        widget.TargetDirectoryLineEdit.setText(currentPath);
    }
}")

        # Create Linux component script with CreateDesktopEntry
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cargonet_component.qs" "
function Component() {
    // Linux component - no admin selection page needed
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        var targetDir = installer.value('TargetDir');
        console.log('DEBUG: Linux - Creating operations for target: ' + targetDir);

        // Create .desktop entry for application menu integration
        component.addOperation('CreateDesktopEntry',
            'CargoNetSim.desktop',
            'Type=Application\\n' +
            'Name=CargoNetSim\\n' +
            'GenericName=Cargo Network Simulator\\n' +
            'Comment=Simulation software for cargo networks\\n' +
            'Exec=' + targetDir + '/bin/CargoNetSim.sh\\n' +
            'Icon=' + targetDir + '/share/icons/hicolor/256x256/apps/CargoNetSim.png\\n' +
            'Terminal=false\\n' +
            'Categories=Science;Simulation;Education;\\n'
        );

        // Update icon cache (best effort - may fail if not root)
        try {
            component.addOperation('Execute', 'gtk-update-icon-cache', '-f', '-t',
                '/usr/share/icons/hicolor',
                'errormessage=Note: Could not update system icon cache');
        } catch (e) {
            console.log('Note: Could not update icon cache: ' + e.message);
        }

    } catch (e) {
        console.log('ERROR in Linux createOperations: ' + e.message);
    }
}")

    endif() # Platform-specific CPack configuration

    # ========================================
    # QtIFW PATH CONFIGURATION (Cross-Platform)
    # ========================================
    if(NOT DEFINED CPACK_IFW_ROOT)
        if(WIN32)
            set(POSSIBLE_IFW_ROOTS
                "C:/Qt/Tools/QtInstallerFramework/4.10"
                "C:/Qt/Tools/QtInstallerFramework/4.8"
                "C:/Qt/Tools/QtInstallerFramework/4.7"
                "C:/Qt/Tools/QtInstallerFramework/4.6"
                "C:/Qt/Tools/QtInstallerFramework/4.1"
                "C:/QtIFW-4.10.0"
                "C:/QtIFW"
                "$ENV{QTIFWDIR}"
            )
            set(IFW_BINARY "binarycreator.exe")
        elseif(APPLE)
            set(POSSIBLE_IFW_ROOTS
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.10"
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.8"
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.7"
                "/opt/Qt/Tools/QtInstallerFramework/4.10"
                "/opt/Qt/Tools/QtInstallerFramework/4.8"
                "/usr/local/opt/qt-installer-framework"
                "/Applications/QtIFW"
                "$ENV{QTIFWDIR}"
            )
            set(IFW_BINARY "binarycreator")
        else() # Linux
            set(POSSIBLE_IFW_ROOTS
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.10"
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.8"
                "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.7"
                "/opt/Qt/Tools/QtInstallerFramework/4.10"
                "/opt/Qt/Tools/QtInstallerFramework/4.8"
                "/usr/local/QtIFW"
                "$ENV{QTIFWDIR}"
            )
            set(IFW_BINARY "binarycreator")
        endif()

        foreach(IFW_PATH ${POSSIBLE_IFW_ROOTS})
            if(EXISTS "${IFW_PATH}/bin/${IFW_BINARY}")
                set(CPACK_IFW_ROOT "${IFW_PATH}")
                message(STATUS "Found QtIFW at: ${IFW_PATH}")
                break()
            endif()
        endforeach()

        if(NOT DEFINED CPACK_IFW_ROOT)
            message(WARNING "QtIFW not found. Please set CPACK_IFW_ROOT manually or install Qt Installer Framework.")
            if(WIN32)
                set(CPACK_IFW_ROOT "C:/Qt/Tools/QtInstallerFramework/4.10")
            elseif(APPLE)
                set(CPACK_IFW_ROOT "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.10")
            else()
                set(CPACK_IFW_ROOT "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.10")
            endif()
        endif()
    endif()

    include(CPack)
    include(CPackIFW)

    # ========================================
    # COMPONENT DEFINITIONS AND CONFIGURATION
    # ========================================

    cpack_add_component(CargoNetSim_COMP
        DISPLAY_NAME "CargoNetSim"
        DESCRIPTION "CargoNetSim simulation software with GUI and all required libraries."
        REQUIRED)

    # Add RabbitMQ Server as optional component
    if(EXISTS "${RABBITMQ_INSTALLER_FILE}")
        cpack_add_component(RabbitMQServer_COMP
            DISPLAY_NAME "RabbitMQ Server"
            DESCRIPTION "RabbitMQ message broker server. Install this if you want this machine to act as the message broker host for CargoNetSim."
            DEPENDS CargoNetSim_COMP)
    endif()

    # Enhanced component configuration with proper versioning and update support
    # Windows has admin selection UI, other platforms do not
    if(WIN32)
        cpack_ifw_configure_component(CargoNetSim_COMP
            ESSENTIAL ON
            FORCED_INSTALLATION ON
            VISIBLE ON
            NAME "com.cargonet.simulator"
            VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
            SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cargonet_component.qs"
            USER_INTERFACES "${CMAKE_CURRENT_BINARY_DIR}/adminselectionform.ui"
            LICENSES "License" ${CPACK_RESOURCE_FILE_LICENSE}
            SORTING_PRIORITY 100)
    else()
        cpack_ifw_configure_component(CargoNetSim_COMP
            ESSENTIAL ON
            FORCED_INSTALLATION ON
            VISIBLE ON
            NAME "com.cargonet.simulator"
            VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
            SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cargonet_component.qs"
            LICENSES "License" ${CPACK_RESOURCE_FILE_LICENSE}
            SORTING_PRIORITY 100)
    endif()

    # Configure RabbitMQ Server component if available (Windows only)
    if(WIN32 AND EXISTS "${RABBITMQ_INSTALLER_FILE}")
        # Create RabbitMQ component script
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq_component.qs" "
function Component() {
    // RabbitMQ Server Component constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        var installType = installer.value('InstallationType');
        console.log('DEBUG: Installing RabbitMQ Server for: ' + installType);

        if (installType === 'SystemWide') {
            // System-wide RabbitMQ installation
            if (installer.fileExists('@TargetDir@/rabbitmq-server-installer.exe')) {
                var result = QMessageBox.question('rabbitmq_install', 'Install RabbitMQ Server',
                    'This will install RabbitMQ Server on your system.\\n\\n' +
                    'RabbitMQ Server is required if this machine will act as the message broker host.\\n\\n' +
                    'Continue with installation?',
                    QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes);

                if (result === QMessageBox.Yes) {
                    component.addOperation('Execute',
                        '@TargetDir@/rabbitmq-server-installer.exe',
                        '/S', // Silent install
                        'UNDOEXECUTE',
                        'wmic', 'product', 'where', 'name=\"RabbitMQ Server\"', 'call', 'uninstall', '/nointeractive');
                }
            }
        } else {
            // User-only installation - provide manual instructions
            QMessageBox.information('rabbitmq_user_install', 'RabbitMQ Server Installation',
                'RabbitMQ Server requires administrator privileges to install.\\n\\n' +
                'The installer has been placed at: @TargetDir@/rabbitmq-server-installer.exe\\n\\n' +
                'Please run it manually with administrator privileges if you want this machine to host the message broker.');
        }
    } catch (e) {
        console.log('ERROR in RabbitMQ createOperations: ' + e.message);
    }
}")

        cpack_ifw_configure_component(RabbitMQServer_COMP
            VISIBLE ON
            NAME "com.cargonet.rabbitmq"
            VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
            SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/rabbitmq_component.qs"
            SORTING_PRIORITY 50)
    endif()

endif()
