# Test executable
add_executable(cobalt_test
    test_runner.c
    
    unit/test_platform.c
    unit/test_atomic.c
    unit/test_allocator.c
    unit/test_arena.c
    unit/test_error.c
    unit/test_logger.c
    unit/test_object.c
    unit/test_class.c
    unit/test_interface.c
    unit/test_vector.c
    unit/test_list.c
    unit/test_hashmap.c
    unit/test_treemap.c
    unit/test_sort.c
    unit/test_functional.c
    unit/test_json.c
    unit/test_eventloop.c
    unit/test_stack.c
    unit/test_queue.c
    unit/test_iterator.c
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
add_test(NAME test_error COMMAND cobalt_test --filter error)
add_test(NAME test_logger COMMAND cobalt_test --filter logger)
add_test(NAME test_object COMMAND cobalt_test --filter object)
add_test(NAME test_class COMMAND cobalt_test --filter class)
add_test(NAME test_interface COMMAND cobalt_test --filter interface)
add_test(NAME test_vector COMMAND cobalt_test --filter vector)
add_test(NAME test_list COMMAND cobalt_test --filter list)
add_test(NAME test_hashmap COMMAND cobalt_test --filter hashmap)
add_test(NAME test_treemap COMMAND cobalt_test --filter treemap)
add_test(NAME test_sort COMMAND cobalt_test --filter sort)
add_test(NAME test_functional COMMAND cobalt_test --filter functional)
add_test(NAME test_json COMMAND cobalt_test --filter json)
add_test(NAME test_eventloop COMMAND cobalt_test --filter eventloop)
add_test(NAME test_stack COMMAND cobalt_test --filter stack)
add_test(NAME test_queue COMMAND cobalt_test --filter queue)
add_test(NAME test_iterator COMMAND cobalt_test --filter iterator)

# Benchmarks
add_executable(benchmark_hashmap benchmark/benchmark_hashmap.c)
target_link_libraries(benchmark_hashmap cobalt)

add_executable(benchmark_vector benchmark/benchmark_vector.c)
target_link_libraries(benchmark_vector cobalt)
