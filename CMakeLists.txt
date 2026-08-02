cmake_minimum_required(VERSION 3.14)
project(Cobalt VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate compile_commands.json for clang-tidy/clang-format
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(COBALT_BUILD_TESTS "Build tests" ON)
option(COBALT_BUILD_EXAMPLES "Build examples" ON)
option(COBALT_RUN_TIDY "Run clang-tidy checks" OFF)
option(COBALT_RUN_FORMAT "Run clang-format checks" OFF)

# Source files
set(SOURCES
    src/platform/platform.c
    src/platform/atomic.c
    src/memory/allocator.c
    src/memory/arena.c
    src/runtime/error.c
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
    src/algorithm/sort.c
    src/algorithm/functional.c
    src/module/json.c
    src/module/eventloop.c
    src/interface/iterator.c
)

# Main library
add_library(cobalt ${SOURCES})

target_include_directories(cobalt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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
target_link_libraries(examples_simple_server cobalt)

# ============================================================
# clang-tidy & clang-format integration
# ============================================================
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
find_program(CLANG_FORMAT_EXECUTABLE clang-format)

if(CLANG_TIDY_EXECUTABLE)
    # Collect all source and header files (exclude build/ and tests/)
    file(GLOB_RECURSE ALL_SRC_FILES
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/include/**/*.h
    )

    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            --warnings-as-errors='*'
            --
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
else()
    message(WARNING "clang-format not found. Install it or skip: cmake --build . --target format")
endif()

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
