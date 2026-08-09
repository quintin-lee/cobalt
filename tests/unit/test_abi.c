/**
 * @file test_abi.c
 * @brief ABI stability test — verifies exported symbols exist and work correctly
 */

#include "test_framework.h"
#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_abi_platform_symbols(void)
{
    printf("Testing platform ABI symbols...\n");
    cobalt_platform_id_t id = cobalt_platform_get_id();
    TEST_ASSERT(id > 0);
    printf("  cobalt_platform_get_id: OK (id=%d)\n", (int)id);
}

void test_abi_atomic_symbols(void)
{
    printf("Testing atomic ABI symbols...\n");
    cobalt_atomic_t a = cobalt_atomic_create(0);
    cobalt_atomic_set(&a, 42);
    TEST_ASSERT(cobalt_atomic_get(&a) == 42);
    cobalt_atomic_increment(&a);
    TEST_ASSERT(cobalt_atomic_get(&a) == 43);
    cobalt_atomic_decrement(&a);
    TEST_ASSERT(cobalt_atomic_get(&a) == 42);
    printf("  Atomic operations: OK\n");
}

void test_abi_thread_symbols(void)
{
    printf("Testing thread ABI symbols...\n");
    cobalt_mutex_t *m = cobalt_mutex_create();
    TEST_ASSERT(m != NULL);
    cobalt_mutex_lock(m);
    cobalt_mutex_unlock(m);
    cobalt_mutex_destroy(m);

    cobalt_cond_t *c = cobalt_cond_create();
    TEST_ASSERT(c != NULL);
    cobalt_cond_destroy(c);
    printf("  Thread primitives: OK\n");
}

void test_abi_allocator_symbols(void)
{
    printf("Testing allocator ABI symbols...\n");
    cobalt_allocator_t *a = cobalt_allocator_get_system();
    void               *p = cobalt_allocator_alloc(a, 64);
    TEST_ASSERT(p != NULL);
    cobalt_allocator_free(a, p);
    printf("  Allocator: OK\n");
}

void test_abi_arena_symbols(void)
{
    printf("Testing arena ABI symbols...\n");
    cobalt_arena_t *arena = cobalt_arena_create(1024);
    TEST_ASSERT(arena != NULL);
    void *p = cobalt_arena_alloc(arena, 64);
    TEST_ASSERT(p != NULL);
    cobalt_arena_destroy(arena);
    printf("  Arena: OK\n");
}

void test_abi_error_symbols(void)
{
    printf("Testing error ABI symbols...\n");
    cobalt_error_t err = COBALT_SUCCESS;
    cobalt_error_set(&err, COBALT_ERROR_NOT_FOUND);
    TEST_ASSERT(err == COBALT_ERROR_NOT_FOUND);
    const char *msg = cobalt_error_get_message(err);
    TEST_ASSERT(msg != NULL);
    printf("  Error handling: OK\n");
}

void test_abi_logger_symbols(void)
{
    printf("Testing logger ABI symbols...\n");
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);
    cobalt_info("ABI test log message\n");
    printf("  Logger: OK\n");
}

void test_abi_vector_symbols(void)
{
    printf("Testing vector ABI symbols...\n");
    cobalt_vector_t *v = cobalt_vector_create(4);
    TEST_ASSERT(v != NULL);
    int val = 42;
    cobalt_vector_push(v, &val);
    TEST_ASSERT(cobalt_vector_size(v) == 1);
    int *got = (int *)cobalt_vector_get(v, 0);
    TEST_ASSERT(got != NULL && *got == 42);
    cobalt_vector_destroy(v);
    printf("  Vector: OK\n");
}

void test_abi_hashmap_symbols(void)
{
    printf("Testing hashmap ABI symbols...\n");
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    TEST_ASSERT(map != NULL);
    int val = 42;
    cobalt_hashmap_put(map, "key", &val);
    int *got = (int *)cobalt_hashmap_get(map, "key");
    TEST_ASSERT(got != NULL && *got == 42);
    cobalt_hashmap_destroy(map);
    printf("  HashMap: OK\n");
}

void test_abi_treemap_symbols(void)
{
    printf("Testing treemap ABI symbols...\n");
    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);
    int val = 42;
    cobalt_treemap_put(map, "key", &val);
    int *got = (int *)cobalt_treemap_get(map, "key");
    TEST_ASSERT(got != NULL && *got == 42);
    cobalt_treemap_destroy(map);
    printf("  TreeMap: OK\n");
}

void test_abi_json_symbols(void)
{
    printf("Testing JSON ABI symbols...\n");
    const char  *json = "{\"a\":1}";
    json_node_t *node = json_parse(json);
    TEST_ASSERT(node != NULL);
    json_destroy(node);
    printf("  JSON: OK\n");
}

void test_abi(void)
{
    printf("Testing ABI stability...\n");
    test_abi_platform_symbols();
    test_abi_atomic_symbols();
    test_abi_thread_symbols();
    test_abi_allocator_symbols();
    test_abi_arena_symbols();
    test_abi_error_symbols();
    test_abi_logger_symbols();
    test_abi_vector_symbols();
    test_abi_hashmap_symbols();
    test_abi_treemap_symbols();
    test_abi_json_symbols();
    printf("  ABI tests completed\n");
}

