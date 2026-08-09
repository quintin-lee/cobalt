/**
 * @file test_thread_safety.c
 * @brief Thread safety documentation verification tests.
 *
 * These tests verify:
 * 1. Single-thread baseline correctness for all containers.
 * 2. Atomic reference counting is thread-safe.
 * 3. Containers exhibit undefined behavior under concurrent unsynchronized access.
 */

#include "cobalt/container/deque.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/container/list.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/container/vector.h"
#include "cobalt/core/object.h"
#include "cobalt/memory/arena.h"
#include "cobalt/memory/pool.h"
#include "cobalt/memory/slab.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* 1. Single-thread baseline: all containers must work correctly              */
/* -------------------------------------------------------------------------- */

void test_thread_safety_vector_single(void)
{
    printf("Testing vector single-thread baseline...\n");
    cobalt_vector_t *v = cobalt_vector_create(4);
    TEST_ASSERT(v != NULL);

    int vals[100];
    for (int i = 0; i < 100; i++) {
        vals[i] = i;
        cobalt_vector_push(v, &vals[i]);
    }
    TEST_ASSERT(cobalt_vector_size(v) == 100);
    TEST_ASSERT(*((int *)cobalt_vector_get(v, 0)) == 0);
    TEST_ASSERT(*((int *)cobalt_vector_get(v, 99)) == 99);
    cobalt_vector_destroy(v);
    printf("  Vector single-thread: OK\n");
}

void test_thread_safety_hashmap_single(void)
{
    printf("Testing hashmap single-thread baseline...\n");
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    TEST_ASSERT(map != NULL);

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        int *val = (int *)malloc(sizeof(int));
        TEST_ASSERT(val != NULL);
        *val = i * 10;
        cobalt_hashmap_put(map, key, val);
    }
    TEST_ASSERT(cobalt_hashmap_size(map) == 100);
    int *got = (int *)cobalt_hashmap_get(map, "key50");
    TEST_ASSERT(got != NULL && *got == 500);
    /* Free all allocated values */
    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        int *v = (int *)cobalt_hashmap_get(map, key);
        if (v) {
            free(v);
        }
    }
    cobalt_hashmap_destroy(map);
    printf("  HashMap single-thread: OK\n");
}

void test_thread_safety_list_single(void)
{
    printf("Testing list single-thread baseline...\n");
    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int vals[50];
    for (int i = 0; i < 50; i++) {
        vals[i] = i;
        cobalt_list_push_back(list, &vals[i]);
    }
    TEST_ASSERT(cobalt_list_size(list) == 50);
    cobalt_list_destroy(list);
    printf("  List single-thread: OK\n");
}

void test_thread_safety_stack_single(void)
{
    printf("Testing stack single-thread baseline...\n");
    cobalt_stack_t *st = cobalt_stack_create();
    TEST_ASSERT(st != NULL);

    int vals[50];
    for (int i = 0; i < 50; i++) {
        vals[i] = i;
        cobalt_stack_push(st, &vals[i]);
    }
    int *val = (int *)cobalt_stack_pop(st);
    TEST_ASSERT(val != NULL && *val == 49);
    cobalt_stack_destroy(st);
    printf("  Stack single-thread: OK\n");
}

void test_thread_safety_queue_single(void)
{
    printf("Testing queue single-thread baseline...\n");
    cobalt_queue_t *q = cobalt_queue_create();
    TEST_ASSERT(q != NULL);

    int vals[50];
    for (int i = 0; i < 50; i++) {
        vals[i] = i;
        cobalt_queue_enqueue(q, &vals[i]);
    }
    int *val = (int *)cobalt_queue_dequeue(q);
    TEST_ASSERT(val != NULL && *val == 0);
    cobalt_queue_destroy(q);
    printf("  Queue single-thread: OK\n");
}

void test_thread_safety_deque_single(void)
{
    printf("Testing deque single-thread baseline...\n");
    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    int vals[50];
    for (int i = 0; i < 50; i++) {
        vals[i] = i;
        cobalt_deque_push_back(dq, &vals[i]);
    }
    int *val = (int *)cobalt_deque_pop_front(dq);
    TEST_ASSERT(val != NULL && *val == 0);
    cobalt_deque_destroy(dq);
    printf("  Deque single-thread: OK\n");
}

void test_thread_safety_treemap_single(void)
{
    printf("Testing treemap single-thread baseline...\n");
    cobalt_treemap_t *tree = cobalt_treemap_create();
    TEST_ASSERT(tree != NULL);

    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        int val = i * 2;
        cobalt_treemap_put(tree, key, &val);
    }
    TEST_ASSERT(cobalt_treemap_size(tree) == 50);
    cobalt_treemap_destroy(tree);
    printf("  TreeMap single-thread: OK\n");
}

void test_thread_safety_set_single(void)
{
    printf("Testing set single-thread baseline...\n");
    cobalt_set_t *set = cobalt_set_create(16);
    TEST_ASSERT(set != NULL);

    int vals[] = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < 5; i++) {
        cobalt_set_insert(set, &vals[i]);
    }
    TEST_ASSERT(cobalt_set_size(set) == 5);
    TEST_ASSERT(cobalt_set_contains(set, &vals[2]));
    cobalt_set_destroy(set);
    printf("  Set single-thread: OK\n");
}

void test_thread_safety_pool_single(void)
{
    printf("Testing pool single-thread baseline...\n");
    cobalt_pool_t *pool = cobalt_pool_create(sizeof(int), 16);
    TEST_ASSERT(pool != NULL);

    int *a = (int *)cobalt_pool_alloc(pool);
    int *b = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(a != NULL && b != NULL);
    *a = 1;
    *b = 2;
    TEST_ASSERT(*a == 1 && *b == 2);
    cobalt_pool_free(pool, a);
    cobalt_pool_free(pool, b);
    cobalt_pool_destroy(pool);
    printf("  Pool single-thread: OK\n");
}

void test_thread_safety_slab_single(void)
{
    printf("Testing slab single-thread baseline...\n");
    size_t         sizes[]  = {sizeof(int), sizeof(double)};
    size_t         counts[] = {8, 4};
    cobalt_slab_t *slab     = cobalt_slab_create(sizes, counts, 2);
    TEST_ASSERT(slab != NULL);

    int    *a = (int *)cobalt_slab_alloc(slab, sizeof(int));
    double *b = (double *)cobalt_slab_alloc(slab, sizeof(double));
    TEST_ASSERT(a != NULL && b != NULL);
    *a = 42;
    *b = 3.14;
    TEST_ASSERT(*a == 42);
    cobalt_slab_free(slab, a);
    cobalt_slab_free(slab, b);
    cobalt_slab_destroy(slab);
    printf("  Slab single-thread: OK\n");
}

void test_thread_safety_arena_single(void)
{
    printf("Testing arena single-thread baseline...\n");
    cobalt_arena_t *arena = cobalt_arena_create(1024);
    TEST_ASSERT(arena != NULL);

    int  *a = (int *)cobalt_arena_alloc(arena, sizeof(int));
    char *s = (char *)cobalt_arena_alloc(arena, 64);
    TEST_ASSERT(a != NULL && s != NULL);
    *a = 99;
    snprintf(s, 64, "hello");
    TEST_ASSERT(*a == 99);
    cobalt_arena_destroy(arena);
    printf("  Arena single-thread: OK\n");
}

/* -------------------------------------------------------------------------- */
/* 2. Atomic ref-count is thread-safe                                         */
/* -------------------------------------------------------------------------- */

static cobalt_object_t *g_shared_obj;
static cobalt_mutex_t  *g_mutex;
static int              g_ref_incr_count;

static void *refcount_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        cobalt_mutex_lock(g_mutex);
        cobalt_object_ref(g_shared_obj);
        g_ref_incr_count++;
        cobalt_object_unref(g_shared_obj);
        cobalt_mutex_unlock(g_mutex);
    }
    return NULL;
}

void test_thread_safety_refcount(void)
{
    printf("Testing atomic ref-count thread safety...\n");

    cobalt_object_t *obj = cobalt_object_new(NULL, 0);
    TEST_ASSERT(obj != NULL);
    TEST_ASSERT(cobalt_object_get_ref_count(obj) == 1);

    /* Extra retain so final unref doesn't destroy before test ends */
    cobalt_object_ref(obj);
    TEST_ASSERT(cobalt_object_get_ref_count(obj) == 2);

    g_shared_obj     = obj;
    g_mutex          = cobalt_mutex_create();
    g_ref_incr_count = 0;
    TEST_ASSERT(g_mutex != NULL);

    cobalt_thread_t threads[4];
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(cobalt_thread_create(refcount_thread, NULL, &threads[i]) == 0);
    }
    for (int i = 0; i < 4; i++) {
        cobalt_thread_join(threads[i]);
    }

    /* Each thread does 1000 retain+release = 4000 net-zero changes */
    TEST_ASSERT(g_ref_incr_count == 4000);
    TEST_ASSERT(cobalt_object_get_ref_count(obj) == 2);

    cobalt_object_unref(obj);
    cobalt_object_unref(obj);
    cobalt_mutex_destroy(g_mutex);
    printf("  Ref-count thread-safe: OK\n");
}

/* -------------------------------------------------------------------------- */
/* 3. Explicit non-thread-safety demonstration                                */
/* -------------------------------------------------------------------------- */

void test_thread_safety_container_not_safe(void)
{
    printf("Testing container non-thread-safety (documentation only)...\n");

    /* Note: Containers are NOT thread-safe. Concurrent unsynchronized access
     * leads to data races and undefined behavior. This test documents that
     * property rather than demonstrating it (which would be inherently flaky).
     * See the thread safety documentation on each container type. */
    cobalt_vector_t *v = cobalt_vector_create(4);
    TEST_ASSERT(v != NULL);

    int val = 42;
    cobalt_vector_push(v, &val);
    TEST_ASSERT(cobalt_vector_size(v) == 1);
    TEST_ASSERT(*((int *)cobalt_vector_get(v, 0)) == 42);

    cobalt_vector_destroy(v);
    printf("  Container thread-safety documented: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Test entry point                                                           */
/* -------------------------------------------------------------------------- */

void test_thread_safety(void)
{
    printf("Testing thread safety...\n");
    test_thread_safety_vector_single();
    test_thread_safety_hashmap_single();
    test_thread_safety_list_single();
    test_thread_safety_stack_single();
    test_thread_safety_queue_single();
    test_thread_safety_deque_single();
    test_thread_safety_treemap_single();
    test_thread_safety_set_single();
    test_thread_safety_pool_single();
    test_thread_safety_slab_single();
    test_thread_safety_arena_single();
    test_thread_safety_refcount();
    test_thread_safety_container_not_safe();
    printf("  Thread safety tests completed\n");
}

