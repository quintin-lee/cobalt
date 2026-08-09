/**
 * @file test_threadsafewrapper.c
 * @brief Unit tests for thread-safe container wrappers
 */

#include "cobalt/container/threadsafewrapper.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================== */
/* Single-thread tests                                                       */
/* ======================================================================== */

void test_threadsafewrapper_vector_basic(void)
{
    printf("Testing thread-safe vector basic operations...\n");
    cobalt_tsvector_t *v = cobalt_tsvector_create(4);
    TEST_ASSERT(v != NULL);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tsvector_push(v, &a) == 0);
    TEST_ASSERT(cobalt_tsvector_push(v, &b) == 0);
    TEST_ASSERT(cobalt_tsvector_push(v, &c) == 0);
    TEST_ASSERT(cobalt_tsvector_size(v) == 3);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 0) == 1);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 1) == 2);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 2) == 3);
    TEST_ASSERT(!cobalt_tsvector_is_empty(v));

    TEST_ASSERT(cobalt_tsvector_set(v, 1, &c) == 0);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 1) == 3);

    cobalt_tsvector_remove_at(v, 1);
    TEST_ASSERT(cobalt_tsvector_size(v) == 2);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 1) == 3);

    cobalt_tsvector_destroy(v);
    printf("  Thread-safe vector basic: OK\n");
}

void test_threadsafewrapper_vector_null(void)
{
    printf("Testing thread-safe vector NULL safety...\n");
    TEST_ASSERT(cobalt_tsvector_push(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tsvector_get(NULL, 0) == NULL);
    TEST_ASSERT(cobalt_tsvector_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsvector_is_empty(NULL) == 1);
    cobalt_tsvector_destroy(NULL); /* must not crash */
    printf("  Thread-safe vector NULL safety: OK\n");
}

void test_threadsafewrapper_vector_allocator(void)
{
    printf("Testing thread-safe vector with custom allocator...\n");
    extern cobalt_allocator_t mock_allocator; /* from test_allocator_inject.c */
    /* We just verify the API accepts a non-NULL allocator */
    cobalt_tsvector_t *v = cobalt_tsvector_create_with_allocator(4, cobalt_allocator_get_system());
    TEST_ASSERT(v != NULL);
    int x = 42;
    TEST_ASSERT(cobalt_tsvector_push(v, &x) == 0);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 0) == 42);
    cobalt_tsvector_destroy(v);
    printf("  Thread-safe vector custom allocator: OK\n");
}

void test_threadsafewrapper_hashmap_basic(void)
{
    printf("Testing thread-safe hashmap basic operations...\n");
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(8);
    TEST_ASSERT(map != NULL);

    int v1 = 100, v2 = 200;
    TEST_ASSERT(cobalt_tshashmap_put(map, "name", &v1) == 0);
    TEST_ASSERT(cobalt_tshashmap_put(map, "age", &v2) == 0);
    TEST_ASSERT(cobalt_tshashmap_size(map) == 2);
    TEST_ASSERT(*(int *)cobalt_tshashmap_get(map, "name") == 100);
    TEST_ASSERT(*(int *)cobalt_tshashmap_get(map, "age") == 200);
    TEST_ASSERT(cobalt_tshashmap_contains(map, "name") == 1);
    TEST_ASSERT(cobalt_tshashmap_contains(map, "missing") == 0);

    TEST_ASSERT(cobalt_tshashmap_remove(map, "name") == 0);
    TEST_ASSERT(cobalt_tshashmap_size(map) == 1);
    TEST_ASSERT(cobalt_tshashmap_get(map, "name") == NULL);

    cobalt_tshashmap_destroy(map);
    printf("  Thread-safe hashmap basic: OK\n");
}

void test_threadsafewrapper_hashmap_null(void)
{
    printf("Testing thread-safe hashmap NULL safety...\n");
    TEST_ASSERT(cobalt_tshashmap_put(NULL, "x", NULL) == -1);
    TEST_ASSERT(cobalt_tshashmap_get(NULL, "x") == NULL);
    TEST_ASSERT(cobalt_tshashmap_contains(NULL, "x") == 0);
    TEST_ASSERT(cobalt_tshashmap_size(NULL) == 0);
    cobalt_tshashmap_destroy(NULL); /* must not crash */
    printf("  Thread-safe hashmap NULL safety: OK\n");
}

void test_threadsafewrapper_list_basic(void)
{
    printf("Testing thread-safe list basic operations...\n");
    cobalt_tslist_t *list = cobalt_tslist_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tslist_push_back(list, &a) == 0);
    TEST_ASSERT(cobalt_tslist_push_back(list, &b) == 0);
    TEST_ASSERT(cobalt_tslist_push_back(list, &c) == 0);
    TEST_ASSERT(cobalt_tslist_size(list) == 3);
    TEST_ASSERT(*(int *)cobalt_tslist_get(list, 0) == 1);
    TEST_ASSERT(*(int *)cobalt_tslist_get(list, 2) == 3);

    void *p = cobalt_tslist_pop_front(list);
    TEST_ASSERT(p == &a);
    TEST_ASSERT(cobalt_tslist_size(list) == 2);

    p = cobalt_tslist_pop_back(list);
    TEST_ASSERT(p == &c);
    TEST_ASSERT(cobalt_tslist_size(list) == 1);

    TEST_ASSERT(cobalt_tslist_is_empty(list) == 0);
    cobalt_tslist_pop_front(list);
    TEST_ASSERT(cobalt_tslist_is_empty(list) == 1);

    cobalt_tslist_destroy(list);
    printf("  Thread-safe list basic: OK\n");
}

void test_threadsafewrapper_list_null(void)
{
    printf("Testing thread-safe list NULL safety...\n");
    TEST_ASSERT(cobalt_tslist_push_front(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tslist_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_tslist_get(NULL, 0) == NULL);
    TEST_ASSERT(cobalt_tslist_size(NULL) == 0);
    TEST_ASSERT(cobalt_tslist_is_empty(NULL) == 1);
    cobalt_tslist_destroy(NULL); /* must not crash */
    printf("  Thread-safe list NULL safety: OK\n");
}

/* ======================================================================== */
/* Multi-thread stress tests                                                 */
/* ======================================================================== */

#define TS_THREAD_COUNT 8
#define TS_ITERATIONS 1000

static void *ts_vector_stress(void *arg)
{
    cobalt_tsvector_t *v   = (cobalt_tsvector_t *)arg;
    int                tid = *(int *)cobalt_tsvector_get(v, 0);
    (void)tid;

    for (int i = 0; i < TS_ITERATIONS; i++) {
        int val = i + tid * TS_ITERATIONS;
        cobalt_tsvector_push(v, &val);
    }
    return NULL;
}

void test_threadsafewrapper_vector_stress(void)
{
    printf("Testing thread-safe vector stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT,
           TS_ITERATIONS);
    cobalt_tsvector_t *v = cobalt_tsvector_create(64);
    TEST_ASSERT(v != NULL);

    /* Store a sentinel at index 0 to pass tid through */
    int sentinel = 0;
    cobalt_tsvector_push(v, &sentinel);

    pthread_t threads[TS_THREAD_COUNT];
    int       tids[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        tids[i] = i;
        cobalt_tsvector_set(v, 0, &tids[i]);
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_vector_stress, v) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    size_t sz = cobalt_tsvector_size(v);
    TEST_ASSERT(sz == 1 + (size_t)TS_THREAD_COUNT * TS_ITERATIONS);
    cobalt_tsvector_destroy(v);
    printf("  Thread-safe vector stress: OK\n");
}

typedef struct {
    cobalt_tshashmap_t *map;
    int                 tid;
} ts_hashmap_ctx_t;

static void *ts_hashmap_stress(void *arg)
{
    ts_hashmap_ctx_t   *ctx = (ts_hashmap_ctx_t *)arg;
    cobalt_tshashmap_t *map = ctx->map;
    int                 tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d_%d", tid, i);
        int *val = (int *)malloc(sizeof(int));
        if (val) {
            *val = i;
            cobalt_tshashmap_put(map, key, val);
        }
    }
    return NULL;
}

void test_threadsafewrapper_hashmap_stress(void)
{
    printf("Testing thread-safe hashmap stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT,
           TS_ITERATIONS);
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(32);
    TEST_ASSERT(map != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_hashmap_ctx_t *ctx = (ts_hashmap_ctx_t *)malloc(sizeof(ts_hashmap_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->map = map;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_hashmap_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    size_t sz = cobalt_tshashmap_size(map);
    TEST_ASSERT(sz == (size_t)TS_THREAD_COUNT * TS_ITERATIONS);

    /* Clean up: release all allocated value pointers */
    for (int t = 0; t < TS_THREAD_COUNT; t++) {
        for (int i = 0; i < TS_ITERATIONS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%d_%d", t + 1, i);
            int *val = (int *)cobalt_tshashmap_get(map, key);
            if (val) {
                free(val);
            }
        }
    }
    cobalt_tshashmap_destroy(map);
    printf("  Thread-safe hashmap stress: OK\n");
}

static void *ts_list_stress(void *arg)
{
    cobalt_tslist_t *list = (cobalt_tslist_t *)arg;
    for (int i = 0; i < TS_ITERATIONS; i++) {
        int *val = (int *)malloc(sizeof(int));
        if (val) {
            *val = i;
            cobalt_tslist_push_back(list, val);
        }
    }
    return NULL;
}

void test_threadsafewrapper_list_stress(void)
{
    printf("Testing thread-safe list stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT,
           TS_ITERATIONS);
    cobalt_tslist_t *list = cobalt_tslist_create();
    TEST_ASSERT(list != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_list_stress, list) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    size_t sz = cobalt_tslist_size(list);
    TEST_ASSERT(sz == (size_t)TS_THREAD_COUNT * TS_ITERATIONS);

    /* Clean up */
    while (cobalt_tslist_size(list) > 0) {
        int *p = (int *)cobalt_tslist_pop_front(list);
        if (p) {
            free(p);
        }
    }
    cobalt_tslist_destroy(list);
    printf("  Thread-safe list stress: OK\n");
}

void test_threadsafewrapper(void)
{
    printf("Testing thread-safe wrappers...\n");
    test_threadsafewrapper_vector_basic();
    test_threadsafewrapper_vector_null();
    test_threadsafewrapper_vector_allocator();
    test_threadsafewrapper_hashmap_basic();
    test_threadsafewrapper_hashmap_null();
    test_threadsafewrapper_list_basic();
    test_threadsafewrapper_list_null();
    test_threadsafewrapper_vector_stress();
    test_threadsafewrapper_hashmap_stress();
    test_threadsafewrapper_list_stress();
    printf("  Thread-safe wrapper tests completed\n");
}
