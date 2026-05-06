# cargonetsim-cli — GUI-free scenario runner.
#
# Structure (Plan 5 Task 3 refactor):
#
#   cargonetsim-cli-lib  STATIC library
#       │   all CLI implementation .cpp + headers
#       │   (Subcommand base, dispatcher, exit codes, later commands,
#       │    writers, progress reporter, ...)
#       │
#       ├── cargonetsim-cli  executable  (main.cpp only)
#       └── tests/CLI/*Test  executables (link the lib to test units
#                                          in isolation)
#
# This split mirrors the src/Backend + src/GUI library/executable split
# used elsewhere in the project. It lets every Plan 5 test link the
# CLI's implementation object code directly (no source-level
# duplication) and keeps main.cpp minimal.
#
# Dependencies:
#   * CargoNetSimBackend  — ScenarioDocument / Serializer / Validator /
#                           Applier / Runtime / ServerStatusProbe
#                           (Plans 1-3). Propagated to consumers as
#                           PUBLIC so tests and the executable both
#                           see it transitively.
#   * Qt6::Core           — QCoreApplication, QString, QFile, QDir, …
#   * yaml-cpp            — propagated transitively via
#                           CargoNetSimBackend.
#
# Qt6 is found at project root via cmake/FindDependencies.cmake — no
# find_package call is needed here.

set(CLI_LIB_SOURCES
    Subcommand.h
    SubcommandDispatcher.h
    SubcommandDispatcher.cpp
    ExitCodes.h
    Output/JsonResultsWriter.h
    Output/JsonResultsWriter.cpp
    Output/CsvResultsWriter.h
    Output/CsvResultsWriter.cpp
    Progress/ProgressReporter.h
    Progress/ProgressReporter.cpp
    Commands/CommandOutput.h
    Commands/IssueFormatter.h
    Commands/HelpVersion.h
    Commands/HelpVersion.cpp
    Commands/ValidateCommand.h
    Commands/ValidateCommand.cpp
    Commands/ConnectionsCommand.h
    Commands/ConnectionsCommand.cpp
    Commands/PreviewCommand.h
    Commands/PreviewCommand.cpp
    Commands/DiscoverCommand.h
    Commands/DiscoverCommand.cpp
    Commands/RunCommand.h
    Commands/RunCommand.cpp
    # AUTORCC (enabled at root) compiles this into the library so
    # `:/cli/help.txt` resolves at runtime. Single source of truth
    # for the help text is `src/CLI/docs/help.txt`.
    docs/cli_resources.qrc
)

add_library(cargonetsim-cli-lib STATIC ${CLI_LIB_SOURCES})

target_link_libraries(cargonetsim-cli-lib PUBLIC
    CargoNetSimBackend
    Qt6::Core
)

# Both CLI and Backend headers live under src/ — a single include
# root covers `#include "CLI/ExitCodes.h"` and
# `#include "Backend/Scenario/...h"`. The build-dir src/ root
# provides the CMake-generated `Version.h`. PUBLIC so the executable
# and every test target inherit the same include roots.
target_include_directories(cargonetsim-cli-lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

add_executable(cargonetsim-cli main.cpp)
target_link_libraries(cargonetsim-cli PRIVATE cargonetsim-cli-lib)

install(TARGETS cargonetsim-cli
    RUNTIME DESTINATION bin
    COMPONENT CargoNetSim_COMP
)
