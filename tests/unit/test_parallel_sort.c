/**
 * @file test_parallel_sort.c
 * @brief Comprehensive unit tests for parallel sorting algorithms
 */

#include "cobalt/module/parallel_sort.h"
#include "cobalt/module/threadpool.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

static void test_parallel_qsort_random(void)
{
    printf("Testing parallel PDQSort on random integers...\n");
    const size_t n   = 100000;
    int         *arr = (int *)malloc(n * sizeof(int));
    TEST_ASSERT(arr != NULL);

    uint32_t seed = 123456789U;
    for (size_t i = 0; i < n; i++) {
        seed   = seed * 1664525U + 1013904223U;
        arr[i] = (int)(seed % 1000000U);
    }

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_parallel_qsort(pool, arr, n, sizeof(int), cmp_int);

    for (size_t i = 1; i < n; i++) {
        TEST_ASSERT(arr[i - 1] <= arr[i]);
    }

    cobalt_threadpool_destroy(pool);
    free(arr);
    printf("  Parallel PDQSort random integers: OK\n");
}

static void test_parallel_qsort_reverse(void)
{
    printf("Testing parallel PDQSort on reverse-sorted integers...\n");
    const size_t n   = 50000;
    int         *arr = (int *)malloc(n * sizeof(int));
    TEST_ASSERT(arr != NULL);

    for (size_t i = 0; i < n; i++) {
        arr[i] = (int)(n - i);
    }

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_parallel_qsort(pool, arr, n, sizeof(int), cmp_int);

    for (size_t i = 1; i < n; i++) {
        TEST_ASSERT(arr[i - 1] <= arr[i]);
    }

    cobalt_threadpool_destroy(pool);
    free(arr);
    printf("  Parallel PDQSort reverse integers: OK\n");
}

static void test_parallel_qsort_all_equal(void)
{
    printf("Testing parallel PDQSort on identical elements...\n");
    const size_t n   = 50000;
    int         *arr = (int *)malloc(n * sizeof(int));
    TEST_ASSERT(arr != NULL);

    for (size_t i = 0; i < n; i++) {
        arr[i] = 42;
    }

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_parallel_qsort(pool, arr, n, sizeof(int), cmp_int);

    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT(arr[i] == 42);
    }

    cobalt_threadpool_destroy(pool);
    free(arr);
    printf("  Parallel PDQSort all equal: OK\n");
}

static void test_parallel_qsort_edge_cases(void)
{
    printf("Testing parallel PDQSort edge cases...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(2);
    TEST_ASSERT(pool != NULL);

    /* NULL pool should fall back to sequential qsort cleanly */
    int small[5] = {5, 2, 4, 1, 3};
    cobalt_parallel_qsort(NULL, small, 5, sizeof(int), cmp_int);
    for (size_t i = 1; i < 5; i++) {
        TEST_ASSERT(small[i - 1] <= small[i]);
    }

    /* NULL base or zero nmemb */
    cobalt_parallel_qsort(pool, NULL, 10, sizeof(int), cmp_int);
    cobalt_parallel_qsort(pool, small, 0, sizeof(int), cmp_int);
    cobalt_parallel_qsort(pool, small, 1, sizeof(int), cmp_int);

    /* Tiny cutoff (force task spawning down to 8 elements) */
    int tiny[100];
    for (size_t i = 0; i < 100; i++) {
        tiny[i] = (int)(100 - i);
    }
    cobalt_parallel_qsort_with_cutoff(pool, tiny, 100, sizeof(int), cmp_int, 8);
    for (size_t i = 1; i < 100; i++) {
        TEST_ASSERT(tiny[i - 1] <= tiny[i]);
    }

    cobalt_threadpool_destroy(pool);
    printf("  Parallel PDQSort edge cases: OK\n");
}

typedef struct {
    int key;
    int original_index;
} stable_item_t;

static int cmp_stable_item(const void *a, const void *b)
{
    const stable_item_t *ia = (const stable_item_t *)a;
    const stable_item_t *ib = (const stable_item_t *)b;
    return (ia->key > ib->key) - (ia->key < ib->key);
}

static void test_parallel_stable_sort_composite(void)
{
    printf("Testing parallel stable sort on 100,000 composite items...\n");
    const size_t   n   = 100000;
    stable_item_t *arr = (stable_item_t *)malloc(n * sizeof(stable_item_t));
    TEST_ASSERT(arr != NULL);

    uint32_t seed = 987654321U;
    for (size_t i = 0; i < n; i++) {
        seed                  = seed * 1664525U + 1013904223U;
        arr[i].key            = (int)(seed % 500U); // Dense duplicates
        arr[i].original_index = (int)i;
    }

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_parallel_stable_sort(pool, arr, n, sizeof(stable_item_t), cmp_stable_item);

    for (size_t i = 1; i < n; i++) {
        TEST_ASSERT(arr[i - 1].key <= arr[i].key);
        if (arr[i - 1].key == arr[i].key) {
            TEST_ASSERT(arr[i - 1].original_index < arr[i].original_index);
        }
    }

    cobalt_threadpool_destroy(pool);
    free(arr);
    printf("  Parallel stable sort 100%% stability verified: OK\n");
}

static void test_parallel_stable_sort_with_alloc(void)
{
    printf("Testing parallel stable sort with custom allocator...\n");
    const size_t n   = 20000;
    int         *arr = (int *)malloc(n * sizeof(int));
    TEST_ASSERT(arr != NULL);

    for (size_t i = 0; i < n; i++) {
        arr[i] = (int)(n - i);
    }

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    cobalt_parallel_stable_sort_with_alloc(pool, arr, n, sizeof(int), cmp_int, alloc);

    for (size_t i = 1; i < n; i++) {
        TEST_ASSERT(arr[i - 1] <= arr[i]);
    }

    cobalt_threadpool_destroy(pool);
    free(arr);
    printf("  Parallel stable sort with custom allocator: OK\n");
}

void test_parallel_sort(void)
{
    printf("=== Starting Parallel Sort Tests ===\n");
    test_parallel_qsort_random();
    test_parallel_qsort_reverse();
    test_parallel_qsort_all_equal();
    test_parallel_qsort_edge_cases();
    test_parallel_stable_sort_composite();
    test_parallel_stable_sort_with_alloc();
    printf("=== Parallel Sort Tests Completed Successfully ===\n");
}
