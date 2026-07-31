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
)

target_include_directories(cobalt_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cobalt_test cobalt)

# On Unix, link pthread for atomic thread-safety tests
if(UNIX)
    target_link_libraries(cobalt_test pthread)
endif()

# Register with CTest
enable_testing()
add_test(NAME CobaltPlatformTest COMMAND cobalt_test)
