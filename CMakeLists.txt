cmake_minimum_required(VERSION 3.14)
project(Cobalt VERSION 2.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate compile_commands.json for clang-tidy/clang-format
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(COBALT_BUILD_TESTS "Build tests" ON)
option(COBALT_BUILD_EXAMPLES "Build examples" ON)
option(COBALT_RUN_TIDY "Run clang-tidy checks" ON)
option(COBALT_RUN_FORMAT "Run clang-format checks" ON)
option(COBALT_BUILD_COVERAGE "Build with coverage instrumentation" OFF)
option(COBALT_COVERAGE_THRESHOLD "Minimum coverage percentage to pass (0 = no check)" 80)
option(COBALT_RUN_ASAN "Build with AddressSanitizer" OFF)

# ASAN build flags
if(COBALT_RUN_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# Copy lsan.suppress to build dir for ASan tests
if(EXISTS ${CMAKE_SOURCE_DIR}/lsan.suppress)
    configure_file(
        ${CMAKE_SOURCE_DIR}/lsan.suppress
        ${CMAKE_BINARY_DIR}/lsan.suppress
        COPYONLY
    )
endif()

# Coverage build flags
if(COBALT_BUILD_COVERAGE)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -O0 -g")
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping -O0 -g")
    endif()
endif()

# clang-tidy & clang-format tools
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
find_program(CLANG_FORMAT_EXECUTABLE clang-format)

# Source files for format/tidy (exclude build/ and tests/)
file(GLOB_RECURSE ALL_SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/include/**/*.h
)

# Source files
set(SOURCES
    src/platform/platform.c
    src/platform/atomic.c
    src/platform/thread.c
    src/memory/allocator.c
    src/memory/arena.c
    src/memory/pool.c
    src/memory/slab.c
    src/runtime/error.c
    src/runtime/error_stack.c
    src/runtime/logger.c
    src/core/object.c
    src/core/class.c
    src/core/interface.c
    src/container/vector.c
    src/container/list.c
    src/container/hashmap.c
    src/container/treemap.c
    src/container/stack.c
    src/container/queue.c
    src/container/set.c
    src/container/deque.c
    src/container/threadsafewrapper.c
    src/algorithm/sort.c
    src/algorithm/functional.c
    src/algorithm/stream.c
    src/module/json.c
    src/module/json_parse.c
    src/module/json_serialize.c
    src/module/eventloop.c
    src/interface/iterator.c
    src/interface/map.c
    src/interface/sequence.c
    src/utils/string.c
    src/utils/benchmark.c
)

# Main library
add_library(cobalt ${SOURCES})
set_target_properties(cobalt PROPERTIES
    LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/cobalt.map"
)

target_include_directories(cobalt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Linker script for test registry sections (used by cobalt_test)
set(COBALT_LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/tests/test_registry.ld)

# Examples
if(COBALT_BUILD_EXAMPLES)
    add_executable(examples_basic_usage examples/basic_usage.c)
    target_link_libraries(examples_basic_usage cobalt)
endif()

# Tests
if(COBALT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Export pkg-config
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cobalt.pc.in
    ${CMAKE_BINARY_DIR}/cobalt.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/cobalt.pc DESTINATION lib/pkgconfig)

# Export targets
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cobaltConfig.cmake.in
    ${CMAKE_BINARY_DIR}/cobaltConfig.cmake
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/cobaltConfig.cmake DESTINATION cmake/Cobalt)

# Install headers
install(DIRECTORY include/cobalt DESTINATION include)

# Install library
install(TARGETS cobalt
    EXPORT CobaltTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Additional examples
add_executable(examples_object_oriented examples/object_oriented.c)
target_link_libraries(examples_object_oriented cobalt)

add_executable(examples_memory_management examples/memory_management.c)
target_link_libraries(examples_memory_management cobalt)

add_executable(examples_containers_demo examples/containers_demo.c)
target_link_libraries(examples_containers_demo cobalt)

add_executable(examples_advanced_usage examples/advanced_usage.c)
target_link_libraries(examples_advanced_usage cobalt)
if(UNIX)
    target_link_libraries(examples_advanced_usage pthread)
endif()

# Additional example programs from docs/EXAMPLES
add_executable(examples_hello_world examples/hello_world.c)
target_link_libraries(examples_hello_world cobalt)

add_executable(examples_vector_demo examples/vector_demo.c)
target_link_libraries(examples_vector_demo cobalt)

add_executable(examples_hashmap_demo examples/hashmap_demo.c)
add_executable(examples_map_demo examples/map_demo.c)
target_link_libraries(examples_map_demo cobalt)
target_link_libraries(examples_hashmap_demo cobalt)

add_executable(examples_arena_allocator examples/arena_allocator.c)
target_link_libraries(examples_arena_allocator cobalt)

add_executable(examples_class_hierarchy examples/class_hierarchy.c)
target_link_libraries(examples_class_hierarchy cobalt)

add_executable(examples_sort_example examples/sort_example.c)
target_link_libraries(examples_sort_example cobalt)

add_executable(examples_json_example examples/json_example.c)
target_link_libraries(examples_json_example cobalt)

add_executable(examples_refcount_demo examples/refcount_demo.c)
target_link_libraries(examples_refcount_demo cobalt pthread)

add_executable(examples_simple_server examples/simple_server.c)

add_executable(examples_error_stack_demo examples/error_stack_demo.c)
target_link_libraries(examples_error_stack_demo cobalt)

add_executable(examples_concurrent_demo examples/concurrent_demo.c)
target_link_libraries(examples_concurrent_demo cobalt pthread)
target_link_libraries(examples_simple_server cobalt)

# ============================================================
# clang-tidy & clang-format integration
# ============================================================
if(CLANG_TIDY_EXECUTABLE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${ALL_SRC_FILES}
        DEPENDS ${ALL_SRC_FILES}
        COMMENT "Running clang-tidy checks..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    message(WARNING "clang-tidy not found. Install it or skip: cmake --build . --target tidy")
endif()

if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE}
            --dry-run --Werror --style=file
            ${ALL_SRC_FILES}
        COMMENT "Running clang-format checks..."
    )

    add_custom_target(format-fix
        COMMAND ${CLANG_FORMAT_EXECUTABLE}
            --style=file -i
            ${ALL_SRC_FILES}
        COMMENT "Fixing code formatting..."
    )

    # Auto-run format check after building cobalt library (only when sources change)
    if(COBALT_RUN_FORMAT)
        add_custom_command(
            TARGET cobalt POST_BUILD
            COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror --style=file ${ALL_SRC_FILES}
            COMMENT "Checking code formatting..."
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
else()
    message(WARNING "clang-format not found. Install it or skip: cmake --build . --target format")
endif()

# Coverage target
add_custom_target(coverage
    COMMAND cmake --build . --parallel
    COMMAND ctest --output-on-failure
    COMMAND lcov --capture --directory . --output-file coverage.info --rc branch_coverage=1 --ignore-errors negative
    COMMAND genhtml coverage.info --output-directory coverage_report --rc branch_coverage=1 --ignore-errors deprecated
    DEPENDS cobalt cobalt_test
    COMMENT "Building, running tests, and generating coverage report..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# Also generate a summary to stdout
add_custom_target(coverage-summary
    COMMAND cmake --build . --parallel
    COMMAND ctest --output-on-failure
    COMMAND lcov --capture --directory . --rc branch_coverage=1 -o coverage.info --ignore-errors negative
    COMMAND genhtml coverage.info --output-directory coverage_report --rc branch_coverage=1 --ignore-errors deprecated
    COMMAND tail -5 coverage_report/index.html | grep -oE '[0-9]+\.[0-9]+%' || true
    DEPENDS cobalt cobalt_test
    COMMENT "Running coverage and printing summary..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# Check coverage against threshold
add_custom_target(coverage-check
    COMMAND cmake --build . --parallel
    COMMAND ctest --output-on-failure
    COMMAND lcov --capture --directory . --rc branch_coverage=1 -o coverage.info --ignore-errors negative
    COMMAND ${CMAKE_SOURCE_DIR}/cmake/check_coverage.sh ${COBALT_COVERAGE_THRESHOLD}
    DEPENDS cobalt cobalt_test
    COMMENT "Running coverage and checking against threshold..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Convenience: run both tidy and format as a single check target
if(CLANG_TIDY_EXECUTABLE AND CLANG_FORMAT_EXECUTABLE)
    add_custom_target(check
        DEPENDS tidy format
        COMMENT "Running all static checks (clang-tidy + clang-format)..."
    )
elseif(CLANG_TIDY_EXECUTABLE)
    add_custom_target(check DEPENDS tidy)
elseif(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(check DEPENDS format)
endif()

# Doxygen documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen..."
        VERBATIM
    )
endif()
