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

/* ======================================================================== */
/* Concurrency helpers                                                       */
/* ======================================================================== */

typedef struct {
    cobalt_tsdeque_t *dq;
    int               tid;
} ts_deque_ctx_t;

static void *ts_deque_stress(void *arg)
{
    ts_deque_ctx_t *ctx = (ts_deque_ctx_t *)arg;
    cobalt_tsdeque_t *dq = ctx->dq;
    int tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        int *val = (int *)malloc(sizeof(int));
        if (!val) break;
        *val = i + tid * TS_ITERATIONS;
        if (i % 3 == 0) {
            cobalt_tsdeque_push_front(dq, val);
        } else {
            cobalt_tsdeque_push_back(dq, val);
        }
        if (cobalt_tsdeque_size(dq) > TS_ITERATIONS) {
            int *p = (int *)cobalt_tsdeque_pop_front(dq);
            if (p) free(p);
            p = (int *)cobalt_tsdeque_pop_back(dq);
            if (p) free(p);
        }
    }
    return NULL;
}

void test_threadsafewrapper_deque_stress(void)
{
    printf("Testing thread-safe deque stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT, TS_ITERATIONS);
    cobalt_tsdeque_t *dq = cobalt_tsdeque_create();
    TEST_ASSERT(dq != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_deque_ctx_t *ctx = (ts_deque_ctx_t *)malloc(sizeof(ts_deque_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->dq = dq;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_deque_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    size_t sz = cobalt_tsdeque_size(dq);
    TEST_ASSERT(sz <= (size_t)TS_THREAD_COUNT * TS_ITERATIONS);
    while (cobalt_tsdeque_size(dq) > 0) {
        int *p = (int *)cobalt_tsdeque_pop_front(dq);
        if (p) free(p);
    }
    cobalt_tsdeque_destroy(dq);
    printf("  Thread-safe deque stress: OK\n");
}

typedef struct {
    cobalt_tsqueue_t *q;
    int               tid;
} ts_queue_ctx_t;

static void *ts_queue_stress(void *arg)
{
    ts_queue_ctx_t *ctx = (ts_queue_ctx_t *)arg;
    cobalt_tsqueue_t *q = ctx->q;
    int tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        int *val = (int *)malloc(sizeof(int));
        if (!val) break;
        *val = i + tid * TS_ITERATIONS;
        cobalt_tsqueue_enqueue(q, val);
        if (cobalt_tsqueue_size(q) > TS_ITERATIONS / 2) {
            int *p = (int *)cobalt_tsqueue_dequeue(q);
            if (p) free(p);
        }
    }
    return NULL;
}

void test_threadsafewrapper_queue_stress(void)
{
    printf("Testing thread-safe queue stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT, TS_ITERATIONS);
    cobalt_tsqueue_t *q = cobalt_tsqueue_create();
    TEST_ASSERT(q != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_queue_ctx_t *ctx = (ts_queue_ctx_t *)malloc(sizeof(ts_queue_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->q = q;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_queue_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    while (cobalt_tsqueue_size(q) > 0) {
        int *p = (int *)cobalt_tsqueue_dequeue(q);
        if (p) free(p);
    }
    cobalt_tsqueue_destroy(q);
    printf("  Thread-safe queue stress: OK\n");
}

typedef struct {
    cobalt_tsstack_t *stk;
    int               tid;
} ts_stack_ctx_t;

static void *ts_stack_stress(void *arg)
{
    ts_stack_ctx_t *ctx = (ts_stack_ctx_t *)arg;
    cobalt_tsstack_t *stk = ctx->stk;
    int tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        int *val = (int *)malloc(sizeof(int));
        if (!val) break;
        *val = i + tid * TS_ITERATIONS;
        cobalt_tsstack_push(stk, val);
        if (cobalt_tsstack_size(stk) > TS_ITERATIONS) {
            int *p = (int *)cobalt_tsstack_pop(stk);
            if (p) free(p);
        }
    }
    return NULL;
}

void test_threadsafewrapper_stack_stress(void)
{
    printf("Testing thread-safe stack stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT, TS_ITERATIONS);
    cobalt_tsstack_t *stk = cobalt_tsstack_create();
    TEST_ASSERT(stk != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_stack_ctx_t *ctx = (ts_stack_ctx_t *)malloc(sizeof(ts_stack_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->stk = stk;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_stack_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    while (cobalt_tsstack_size(stk) > 0) {
        int *p = (int *)cobalt_tsstack_pop(stk);
        if (p) free(p);
    }
    cobalt_tsstack_destroy(stk);
    printf("  Thread-safe stack stress: OK\n");
}

typedef struct {
    cobalt_tstreemap_t *tm;
    int                 tid;
} ts_treemap_ctx_t;

static void *ts_treemap_stress(void *arg)
{
    ts_treemap_ctx_t   *ctx = (ts_treemap_ctx_t *)arg;
    cobalt_tstreemap_t *tm  = ctx->tm;
    int                 tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        char key[64];
        snprintf(key, sizeof(key), "tmkey_%d_%d", tid, i);
        int *val = (int *)malloc(sizeof(int));
        if (!val) break;
        *val = i;
        cobalt_tstreemap_put(tm, key, val);
        cobalt_tstreemap_get(tm, key);
        if (i % 7 == 0) {
            void *removed = cobalt_tstreemap_get(tm, key);
            if (removed) {
                free(removed);
            }
            cobalt_tstreemap_remove(tm, key);
        }
    }
    return NULL;
}

void test_threadsafewrapper_treemap_stress(void)
{
    printf("Testing thread-safe treemap stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT, TS_ITERATIONS);
    cobalt_tstreemap_t *tm = cobalt_tstreemap_create();
    TEST_ASSERT(tm != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_treemap_ctx_t *ctx = (ts_treemap_ctx_t *)malloc(sizeof(ts_treemap_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->tm = tm;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_treemap_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Clean up: release all allocated value pointers */
    for (int t = 0; t < TS_THREAD_COUNT; t++) {
        for (int i = 0; i < TS_ITERATIONS; i++) {
            char key[64];
            snprintf(key, sizeof(key), "tmkey_%d_%d", t + 1, i);
            int *val = (int *)cobalt_tstreemap_get(tm, key);
            if (val) {
                free(val);
            }
        }
    }

    TEST_ASSERT(cobalt_tstreemap_size(tm) > 0);
    cobalt_tstreemap_destroy(tm);
    printf("  Thread-safe treemap stress: OK\n");
}

typedef struct {
    cobalt_tsset_t *ss;
    int             tid;
} ts_set_ctx_t;

static void *ts_set_stress(void *arg)
{
    ts_set_ctx_t *ctx = (ts_set_ctx_t *)arg;
    cobalt_tsset_t *ss = ctx->ss;
    int tid = ctx->tid;
    free(ctx);
    for (int i = 0; i < TS_ITERATIONS; i++) {
        char item[64];
        snprintf(item, sizeof(item), "item_%d_%d", tid, i);
        cobalt_tsset_insert(ss, item);
        cobalt_tsset_contains(ss, item);
        if (i % 5 == 0) {
            cobalt_tsset_remove(ss, item);
        }
    }
    return NULL;
}

void test_threadsafewrapper_set_stress(void)
{
    printf("Testing thread-safe set stress (%d threads x %d iters)...\n",
           TS_THREAD_COUNT, TS_ITERATIONS);
    cobalt_tsset_t *ss = cobalt_tsset_create(64);
    TEST_ASSERT(ss != NULL);

    pthread_t threads[TS_THREAD_COUNT];
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        ts_set_ctx_t *ctx = (ts_set_ctx_t *)malloc(sizeof(ts_set_ctx_t));
        TEST_ASSERT(ctx != NULL);
        ctx->ss = ss;
        ctx->tid = i + 1;
        TEST_ASSERT(pthread_create(&threads[i], NULL, ts_set_stress, ctx) == 0);
    }
    for (int i = 0; i < TS_THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    TEST_ASSERT(cobalt_tsset_size(ss) > 0);
    cobalt_tsset_destroy(ss);
    printf("  Thread-safe set stress: OK\n");
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
    test_threadsafewrapper_deque_stress();
    test_threadsafewrapper_queue_stress();
    test_threadsafewrapper_stack_stress();
    test_threadsafewrapper_treemap_stress();
    test_threadsafewrapper_set_stress();
    test_threadsafewrapper_vector_stress();
    test_threadsafewrapper_hashmap_stress();
    test_threadsafewrapper_list_stress();
    printf("  Thread-safe wrapper tests completed\n");
}
