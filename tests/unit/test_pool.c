/**
 * @file test_pool.c
 * @brief Unit test for pool allocator.
 */

#include "cobalt/memory/pool.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdint.h>

void test_pool_create_alloc(void)
{
    printf("Testing pool create and alloc...\n");

    cobalt_pool_t *pool = cobalt_pool_create(sizeof(int), 8);
    TEST_ASSERT(pool != NULL);

    int *a = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(a != NULL);
    *a = 42;
    TEST_ASSERT(*a == 42);

    cobalt_pool_destroy(pool);
    printf("  Create/alloc: OK\n");
}

void test_pool_alloc_free_alloc(void)
{
    printf("Testing alloc-free-alloc reuse...\n");

    cobalt_pool_t *pool = cobalt_pool_create(sizeof(int), 4);
    TEST_ASSERT(pool != NULL);

    int *a = (int *)cobalt_pool_alloc(pool);
    int *b = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(a != NULL && b != NULL);
    TEST_ASSERT(a != b);

    *a = 1;
    *b = 2;
    cobalt_pool_free(pool, a);

    int *c = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(c != NULL);
    /* c may be the same as a (reused) */
    *c = 3;

    cobalt_pool_free(pool, b);
    cobalt_pool_free(pool, c);
    cobalt_pool_destroy(pool);
    printf("  Alloc-free-alloc: OK\n");
}

void test_pool_full(void)
{
    printf("Testing pool full condition...\n");

    cobalt_pool_t *pool = cobalt_pool_create(sizeof(int), 2);
    TEST_ASSERT(pool != NULL);
    TEST_ASSERT(cobalt_pool_free_count(pool) == 2);

    int *a = (int *)cobalt_pool_alloc(pool);
    int *b = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(a != NULL && b != NULL);
    TEST_ASSERT(cobalt_pool_is_full(pool) == 1);
    TEST_ASSERT(cobalt_pool_free_count(pool) == 0);

    /* Should return NULL when full */
    TEST_ASSERT(cobalt_pool_alloc(pool) == NULL);

    cobalt_pool_free(pool, a);
    TEST_ASSERT(cobalt_pool_is_full(pool) == 0);
    TEST_ASSERT(cobalt_pool_free_count(pool) == 1);

    cobalt_pool_destroy(pool);
    printf("  Full condition: OK\n");
}

void test_pool_null_safe(void)
{
    printf("Testing null safety...\n");

    cobalt_pool_destroy(NULL);
    cobalt_pool_free(NULL, NULL);
    TEST_ASSERT(cobalt_pool_is_full(NULL) == 1);
    TEST_ASSERT(cobalt_pool_free_count(NULL) == 0);
    TEST_ASSERT(cobalt_pool_alloc(NULL) == NULL);
    printf("  Null safety: OK\n");
}

void test_pool_alignment(void)
{
    printf("Testing block alignment...\n");

    cobalt_pool_t *pool = cobalt_pool_create(64, 4);
    TEST_ASSERT(pool != NULL);

    for (int i = 0; i < 4; i++) {
        void *ptr = cobalt_pool_alloc(pool);
        TEST_ASSERT(ptr != NULL);
        TEST_ASSERT(((uintptr_t)ptr % 8) == 0);
    }

    cobalt_pool_destroy(pool);
    printf("  Alignment: OK\n");
}

void test_pool(void)
{
    printf("Testing pool allocator...\n");
    test_pool_create_alloc();
    test_pool_alloc_free_alloc();
    test_pool_full();
    test_pool_null_safe();
    test_pool_alignment();
    printf("  Pool tests completed\n");
}

