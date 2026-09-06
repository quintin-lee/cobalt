# Test executable
# Auto-discover unit tests
file(GLOB UNIT_TESTS "unit/test_*.c")

add_executable(cobalt_test
    test_runner.c
    ${UNIT_TESTS}
)

target_include_directories(cobalt_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(cobalt_test cobalt)


# On Unix, link pthread for atomic thread-safety tests
if(UNIX)
    target_link_libraries(cobalt_test pthread)
endif()

# Register with CTest (done in main CMakeLists.txt)

# Add individual test cases for CTest verification
add_test(NAME test_platform COMMAND cobalt_test --filter platform)
add_test(NAME test_atomic COMMAND cobalt_test --filter atomic)
add_test(NAME test_allocator COMMAND cobalt_test --filter allocator)
add_test(NAME test_arena COMMAND cobalt_test --filter arena)
add_test(NAME test_pool COMMAND cobalt_test --filter pool)
add_test(NAME test_slab COMMAND cobalt_test --filter slab)
add_test(NAME test_error COMMAND cobalt_test --filter error)
add_test(NAME test_error_stack COMMAND cobalt_test --filter error_stack)
add_test(NAME test_logger COMMAND cobalt_test --filter logger)
add_test(NAME test_object COMMAND cobalt_test --filter object)
add_test(NAME test_class COMMAND cobalt_test --filter class)
add_test(NAME test_interface COMMAND cobalt_test --filter interface)
add_test(NAME test_vector COMMAND cobalt_test --filter vector)
add_test(NAME test_list COMMAND cobalt_test --filter list)
add_test(NAME test_hashmap COMMAND cobalt_test --filter hashmap)
add_test(NAME test_treemap COMMAND cobalt_test --filter treemap)
add_test(NAME test_string COMMAND cobalt_test --filter string)
add_test(NAME test_functional COMMAND cobalt_test --filter functional)
add_test(NAME test_stream COMMAND cobalt_test --filter stream)
add_test(NAME test_json COMMAND cobalt_test --filter json)
if(NOT WIN32)
add_test(NAME test_eventloop COMMAND cobalt_test --filter eventloop)
endif()
add_test(NAME test_stack COMMAND cobalt_test --filter stack)
add_test(NAME test_queue COMMAND cobalt_test --filter queue)
add_test(NAME test_set COMMAND cobalt_test --filter set)
add_test(NAME test_deque COMMAND cobalt_test --filter deque)
add_test(NAME test_sequence COMMAND cobalt_test --filter sequence)
add_test(NAME test_iterator COMMAND cobalt_test --filter iterator)
add_test(NAME test_map COMMAND cobalt_test --filter map)
add_test(NAME test_thread COMMAND cobalt_test --filter thread)
add_test(NAME test_platform_utils COMMAND cobalt_test --filter platform_utils)
add_test(NAME test_abi COMMAND cobalt_test --filter abi)
add_test(NAME test_allocator_inject COMMAND cobalt_test --filter allocator_inject)
# Set LSAN suppressions for ASan builds
if(EXISTS ${CMAKE_BINARY_DIR}/lsan.suppress)
    set_tests_properties(test_allocator_inject PROPERTIES
        ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_BINARY_DIR}/lsan.suppress"
    )
endif()
add_test(NAME test_thread_safety COMMAND cobalt_test --filter thread_safety)
add_test(NAME test_threadsafewrapper COMMAND cobalt_test --filter threadsafewrapper)

# Benchmarks
add_executable(benchmark_hashmap benchmark/benchmark_hashmap.c)
target_link_libraries(benchmark_hashmap cobalt)

add_executable(benchmark_vector benchmark/benchmark_vector.c)
target_link_libraries(benchmark_vector cobalt)

add_executable(benchmark_set benchmark/benchmark_set.c)
add_executable(benchmark_set_map benchmark/benchmark_set_map.c)
target_link_libraries(benchmark_set_map cobalt)
target_link_libraries(benchmark_set cobalt)

add_executable(benchmark_deque benchmark/benchmark_deque.c)
target_link_libraries(benchmark_deque cobalt)

add_executable(benchmark_treemap benchmark/benchmark_treemap.c)
add_executable(benchmark_map benchmark/benchmark_map.c)
target_link_libraries(benchmark_map cobalt)
target_link_libraries(benchmark_treemap cobalt)

add_executable(benchmark_threadsafewrapper benchmark/benchmark_threadsafewrapper.c)
target_link_libraries(benchmark_threadsafewrapper cobalt pthread m)

# Register benchmarks with CTest
add_test(NAME bench_vector COMMAND benchmark_vector
         PASS_REGULAR_EXPRESSION "Benchmark completed")
add_test(NAME bench_hashmap COMMAND benchmark_hashmap
         PASS_REGULAR_EXPRESSION "Benchmark completed")
add_test(NAME bench_treemap COMMAND benchmark_treemap
         PASS_REGULAR_EXPRESSION "Benchmark completed")
add_test(NAME bench_set COMMAND benchmark_set
         PASS_REGULAR_EXPRESSION "Benchmark completed")
add_test(NAME bench_deque COMMAND benchmark_deque
         PASS_REGULAR_EXPRESSION "Benchmark completed")
add_test(NAME bench_threadsafewrapper COMMAND benchmark_threadsafewrapper
         PASS_REGULAR_EXPRESSION "Benchmark suite complete")

# Unified benchmark runner
add_executable(bench_runner benchmark/bench_runner.c)
target_link_libraries(bench_runner cobalt m)

add_test(NAME bench_runner COMMAND bench_runner
         PASS_REGULAR_EXPRESSION "Benchmark suite complete")
