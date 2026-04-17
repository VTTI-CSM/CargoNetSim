add_subdirectory(Backend)
add_subdirectory(GUI)
add_subdirectory(CLI)

# Conditionally add RabbitMQ config tool
if(CARGONET_BUILD_RABBITMQ_CONFIG)
    add_subdirectory(rabbitmq-config)
endif()

# Define the resources variable
set(RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../resources/CargoNetSim.qrc
)

qt6_add_resources(RESOURCE_SOURCES ${RESOURCES})

set(SOURCES
    main.cpp    # Main file
    ${RESOURCE_SOURCES}  # Include the compiled resources
)

# Add icon for Windows executable
if(WIN32)
    # Create a resource file for the icon
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/CargoNetSim.rc")
    file(WRITE ${RC_FILE} "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/../resources/images/favicon.ico\"\n")
    list(APPEND SOURCES ${RC_FILE})
endif()


add_executable(CargoNetSim ${SOURCES})

# macOS Bundle Configuration
if(APPLE)
    set_target_properties(CargoNetSim PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.cargonet.simulator"
        MACOSX_BUNDLE_BUNDLE_NAME "CargoNetSim"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "CargoNetSim.icns"
    )

    # Install macOS icon if it exists
    set(MACOS_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../resources/images/CargoNetSim.icns")
    if(EXISTS "${MACOS_ICON_PATH}")
        set_source_files_properties(${MACOS_ICON_PATH} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(CargoNetSim PRIVATE ${MACOS_ICON_PATH})
    else()
        message(WARNING "macOS icon not found at: ${MACOS_ICON_PATH}")
    endif()
endif()

target_link_libraries(CargoNetSim
    PRIVATE
    CargoNetSimBackend
    CargoNetSimGUI
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Container::Container
    rabbitmq::rabbitmq
)

# Installation
install(TARGETS CargoNetSim
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
    COMPONENT CargoNetSim_COMP
)

# Copy required DLLs on Windows
if(WIN32)
    # Add commands to copy DLLs to build directory
    add_custom_command(TARGET CargoNetSim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RABBITMQ_SHRD_LIB_DIR}"
        $<TARGET_FILE_DIR:CargoNetSim>
    )
    
    # Install the DLLs too
    install(DIRECTORY ${RABBITMQ_SHRD_LIB_DIR}/
        DESTINATION bin
        FILES_MATCHING PATTERN "*.dll"
        COMPONENT CargoNetSim_COMP
    )
endif()
