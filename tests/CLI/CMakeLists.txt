# CLI test executables — populated incrementally by Plan 5 Tasks 2-20.
#
# Each test binary follows the same convention as the rest of the suite:
#   * RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_BINARY_DIR}/bin
#   * Include paths inherited from cargonetsim-cli-lib's PUBLIC dirs
#     (src/ + build/src/), so `#include "CLI/ExitCodes.h"` and
#     `#include "Backend/Scenario/..."` resolve naturally.
#   * Qt6::Core + Qt6::Test are inherited from the parent CMakeLists.
#   * No add_test()/CTest wiring — direct binary invocation.

# Task 2: ExitCodes contract lock-test.
add_executable(ExitCodesTest ExitCodesTest.cpp)
target_link_libraries(ExitCodesTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(ExitCodesTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 3: Subcommand + SubcommandDispatcher routing tests.
# Links the CLI library to exercise dispatcher object code directly
# (no source-level duplication). Include roots inherit from
# cargonetsim-cli-lib's PUBLIC target_include_directories.
add_executable(SubcommandDispatcherTest SubcommandDispatcherTest.cpp)
target_link_libraries(SubcommandDispatcherTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(SubcommandDispatcherTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 6: JsonResultsWriter contract lock-tests.
add_executable(JsonResultsWriterTest JsonResultsWriterTest.cpp)
target_link_libraries(JsonResultsWriterTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(JsonResultsWriterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 7: CsvResultsWriter contract lock-tests.
add_executable(CsvResultsWriterTest CsvResultsWriterTest.cpp)
target_link_libraries(CsvResultsWriterTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(CsvResultsWriterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 8: ProgressReporter rate-limit + quiet-mode lock-tests.
add_executable(ProgressReporterTest ProgressReporterTest.cpp)
target_link_libraries(ProgressReporterTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(ProgressReporterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 9: ControlChannel NDJSON IPC round-trip + error-path tests.
# Needs Qt6::Network for QLocalServer / QLocalSocket — those modules
# are pulled transitively through cargonetsim-cli-lib (which links
# them PUBLIC), so listing the lib alone is sufficient.
add_executable(ControlChannelTest ControlChannelTest.cpp)
target_link_libraries(ControlChannelTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(ControlChannelTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 10: RuntimeStateFile lifecycle + env-override + scan tests.
# Each test uses its own QTemporaryDir + qputenv override for full
# isolation from other tests and any leftover on-disk state.
add_executable(RuntimeStateFileTest RuntimeStateFileTest.cpp)
target_link_libraries(RuntimeStateFileTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(RuntimeStateFileTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 11: HelpCommand + VersionCommand output lock-tests.
# Tests inject a QBuffer to inspect emitted bytes without process
# spawning; the production wiring (Task 18 main.cpp) uses the default
# stdout sink.
add_executable(HelpVersionTest HelpVersionTest.cpp)
target_link_libraries(HelpVersionTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(HelpVersionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 12: ValidateCommand — parse → validate → emit issues → exit.
# Uses the scenario fixtures (minimal.yml, invalid_bad_terminal_type.yml)
# so a POST_BUILD rule mirrors the backend-test convention and copies
# the fixtures directory next to the binary.
add_executable(ValidateCommandTest ValidateCommandTest.cpp)
target_link_libraries(ValidateCommandTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(ValidateCommandTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET ValidateCommandTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Task 13: ConnectionsCommand one-shot tests. Injects a lambda poller
# so no live RabbitMQ / CargoNetSimController is needed. The --watch
# path (timing + SIGINT) is covered by Task 20's E2E under real wall
# time and is intentionally not unit-tested here.
add_executable(ConnectionsCommandTest ConnectionsCommandTest.cpp)
target_link_libraries(ConnectionsCommandTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(ConnectionsCommandTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 14: PreviewCommand — load → validate → link → emit JSON.
# Uses the scenario fixtures (minimal.yml, invalid_bad_terminal_type.yml)
# so it attaches the same POST_BUILD copy as ValidateCommandTest.
# copy_directory is idempotent — concurrent copies from sibling tests
# are safe.
add_executable(PreviewCommandTest PreviewCommandTest.cpp)
target_link_libraries(PreviewCommandTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(PreviewCommandTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET PreviewCommandTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)

# Task 15: StatusCommand integration tests. Spins up a real
# ControlChannelServer in-process + real RuntimeStateFile entries in
# a QTemporaryDir so the scan ↔ state-file ↔ socket integration path
# is exercised, not just each piece mocked.
add_executable(StatusCommandTest StatusCommandTest.cpp)
target_link_libraries(StatusCommandTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(StatusCommandTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 16: StopCommand integration tests. Same real-env pattern as
# StatusCommandTest — real server, real state files, real sockets.
add_executable(StopCommandTest StopCommandTest.cpp)
target_link_libraries(StopCommandTest PRIVATE
    cargonetsim-cli-lib
    Qt6::Test
)
set_target_properties(StopCommandTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 19: end-to-end help-text snapshot test. Spawns the production
# `cargonetsim-cli` binary via QProcess and diffs --help stdout
# against the committed tools/cli/docs/help.txt. The binary's
# absolute path is injected at compile time via
# `target_compile_definitions` so the test does not depend on the
# working directory or CMake build layout. `add_dependencies` makes
# the executable a build-order prerequisite of the test target —
# `cmake --build build --target HelpTextSnapshotTest` always builds
# the binary first.
add_executable(HelpTextSnapshotTest HelpTextSnapshotTest.cpp)
target_link_libraries(HelpTextSnapshotTest PRIVATE
    Qt6::Core
    Qt6::Test
)
target_compile_definitions(HelpTextSnapshotTest PRIVATE
    CARGONETSIM_CLI_BINARY="$<TARGET_FILE:cargonetsim-cli>"
)
add_dependencies(HelpTextSnapshotTest cargonetsim-cli)
set_target_properties(HelpTextSnapshotTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Task 20: end-to-end `cargonetsim-cli run` integration test, gated
# on CARGONETSIM_INTEGRATION=1 + a live RabbitMQ broker + the four
# simulator processes. Self-skips otherwise so CI without the live
# stack stays green. Fixture POST_BUILD copy mirrors the convention
# every fixture-using test in this directory follows.
add_executable(EndToEndRunTest EndToEndRunTest.cpp)
target_link_libraries(EndToEndRunTest PRIVATE
    Qt6::Core
    Qt6::Test
)
target_compile_definitions(EndToEndRunTest PRIVATE
    CARGONETSIM_CLI_BINARY="$<TARGET_FILE:cargonetsim-cli>"
)
add_dependencies(EndToEndRunTest cargonetsim-cli)
set_target_properties(EndToEndRunTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_custom_command(
    TARGET EndToEndRunTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/fixtures
        ${CMAKE_BINARY_DIR}/bin/fixtures
)
