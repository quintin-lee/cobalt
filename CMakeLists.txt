cmake_minimum_required(VERSION 3.14)
project(Cobalt VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(COBALT_BUILD_TESTS "Build tests" ON)
option(COBALT_BUILD_EXAMPLES "Build examples" ON)

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
