cmake_minimum_required(VERSION 3.14)
project(Cobalt VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

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
    src/interface/iterator.c
    src/container/list.c
    src/container/hashmap.c
    src/container/treemap.c
    src/algorithm/sort.c
    src/algorithm/functional.c
    src/module/json.c
    src/module/eventloop.c
)

# Main library
add_library(cobalt ${SOURCES})

target_include_directories(cobalt PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# CTest testing enable
enable_testing()

# Export pkg-config
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cobalt.pc.in
    ${CMAKE_BINARY_DIR}/cobalt.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/cobalt.pc DESTINATION lib/pkgconfig)

# Export targets to config file
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cobaltConfig.cmake.in
    ${CMAKE_BINARY_DIR}/cobaltConfig.cmake
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/cobaltConfig.cmake DESTINATION cmake/Cobalt)

# Testing - build test executable and register with CTest
add_subdirectory(tests)
