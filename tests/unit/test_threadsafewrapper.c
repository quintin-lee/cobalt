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
    TEST_ASSERT(cobalt_tsvector_set(NULL, 0, NULL) == -1);
    TEST_ASSERT(cobalt_tsvector_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsvector_is_empty(NULL) == 1);
    TEST_ASSERT(cobalt_tsvector_capacity(NULL) == 0);
    TEST_ASSERT(cobalt_tsvector_reserve(NULL, 8) == -1);
    TEST_ASSERT(cobalt_tsvector_shrink_to_fit(NULL) == -1);
    cobalt_tsvector_remove_at(NULL, 0); /* must not crash */
    cobalt_tsvector_destroy(NULL);      /* must not crash */
    printf("  Thread-safe vector NULL safety: OK\n");
}

void test_threadsafewrapper_vector_bounds(void)
{
    printf("Testing thread-safe vector bounded access...\n");
    cobalt_tsvector_t *v = cobalt_tsvector_create(2);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(cobalt_tsvector_is_empty(v) == 1);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tsvector_push(v, &a) == 0);
    TEST_ASSERT(cobalt_tsvector_push(v, &b) == 0);

    /* Out-of-range reads return NULL; out-of-range writes fail */
    TEST_ASSERT(cobalt_tsvector_get(v, 2) == NULL);
    TEST_ASSERT(cobalt_tsvector_get(v, 100) == NULL);
    TEST_ASSERT(cobalt_tsvector_set(v, 2, &c) == -1);
    TEST_ASSERT(cobalt_tsvector_set(v, 100, &c) == -1);

    /* In-range access still works after the rejected writes */
    TEST_ASSERT(cobalt_tsvector_size(v) == 2);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 0) == 1);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 1) == 2);

    /* Out-of-range remove_at is a no-op */
    cobalt_tsvector_remove_at(v, 100);
    TEST_ASSERT(cobalt_tsvector_size(v) == 2);

    /* In-range remove_at removes and shifts */
    cobalt_tsvector_remove_at(v, 0);
    TEST_ASSERT(cobalt_tsvector_size(v) == 1);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 0) == 2);

    /* reserve / shrink_to_fit round trip */
    TEST_ASSERT(cobalt_tsvector_reserve(v, 16) == 0);
    TEST_ASSERT(cobalt_tsvector_capacity(v) >= 16);
    TEST_ASSERT(cobalt_tsvector_shrink_to_fit(v) == 0);
    TEST_ASSERT(cobalt_tsvector_capacity(v) == 1);

    cobalt_tsvector_destroy(v);
    printf("  Thread-safe vector bounded access: OK\n");
}

void test_threadsafewrapper_vector_allocator(void)
{
    printf("Testing thread-safe vector with custom allocator...\n");
    /* We just verify the API accepts a non-NULL allocator */
    cobalt_tsvector_t *v = cobalt_tsvector_create_with_allocator(4, cobalt_allocator_get_system());
    TEST_ASSERT(v != NULL);
    int x = 42;
    TEST_ASSERT(cobalt_tsvector_push(v, &x) == 0);
    TEST_ASSERT(*(int *)cobalt_tsvector_get(v, 0) == 42);
    cobalt_tsvector_destroy(v);
    printf("  Thread-safe vector custom allocator: OK\n");
}

void test_threadsafewrapper_ts_allocator_variants(void)
{
    printf("Testing thread-safe wrappers with custom allocator...\n");

    /* Verified against threadsafewrapper.c: this family requires a non-NULL
       allocator and returns NULL otherwise. That is the opposite of the
       containers (arena/treemap/lfqueue), which also reject NULL, and of
       taskgraph, which silently falls back to malloc. Any future change to
       that contract breaks here. */
    TEST_ASSERT(cobalt_tsdeque_create_with_allocator(NULL) == NULL);
    TEST_ASSERT(cobalt_tshashmap_create_with_allocator(8, NULL) == NULL);
    TEST_ASSERT(cobalt_tslist_create_with_allocator(NULL) == NULL);
    TEST_ASSERT(cobalt_tsqueue_create_with_allocator(NULL) == NULL);
    TEST_ASSERT(cobalt_tsset_create_with_allocator(8, NULL) == NULL);
    TEST_ASSERT(cobalt_tsstack_create_with_allocator(NULL) == NULL);
    TEST_ASSERT(cobalt_tstreemap_create_with_allocator(NULL) == NULL);

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    /* System-allocator round trips: create, exercise one op, destroy */
    cobalt_tslist_t *list = cobalt_tslist_create_with_allocator(sys);
    TEST_ASSERT(list != NULL);
    int a = 1;
    TEST_ASSERT(cobalt_tslist_push_back(list, &a) == 0);
    TEST_EQUAL(cobalt_tslist_size(list), (size_t)1);
    cobalt_tslist_destroy(list);

    cobalt_tsstack_t *stk = cobalt_tsstack_create_with_allocator(sys);
    TEST_ASSERT(stk != NULL);
    TEST_ASSERT(cobalt_tsstack_push(stk, &a) == 0);
    TEST_EQUAL(cobalt_tsstack_size(stk), (size_t)1);
    cobalt_tsstack_destroy(stk);

    cobalt_tsqueue_t *q = cobalt_tsqueue_create_with_allocator(sys);
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(cobalt_tsqueue_enqueue(q, &a) == 0);
    TEST_EQUAL(cobalt_tsqueue_size(q), (size_t)1);
    cobalt_tsqueue_destroy(q);

    cobalt_tsdeque_t *dq = cobalt_tsdeque_create_with_allocator(sys);
    TEST_ASSERT(dq != NULL);
    TEST_ASSERT(cobalt_tsdeque_push_back(dq, &a) == 0);
    TEST_EQUAL(cobalt_tsdeque_size(dq), (size_t)1);
    cobalt_tsdeque_destroy(dq);

    cobalt_tshashmap_t *map = cobalt_tshashmap_create_with_allocator(8, sys);
    TEST_ASSERT(map != NULL);
    int v1 = 5;
    TEST_ASSERT(cobalt_tshashmap_put(map, "k", &v1) == 0);
    TEST_EQUAL(cobalt_tshashmap_size(map), (size_t)1);
    cobalt_tshashmap_destroy(map);

    cobalt_tsset_t *ss = cobalt_tsset_create_with_allocator(8, sys);
    TEST_ASSERT(ss != NULL);
    TEST_ASSERT(cobalt_tsset_insert(ss, "m") == 0);
    TEST_EQUAL(cobalt_tsset_size(ss), (size_t)1);
    cobalt_tsset_destroy(ss);

    /* create_ext delegates through create(); verify the comparator path works */
    cobalt_tstreemap_t *tm = cobalt_tstreemap_create_ext(NULL);
    TEST_ASSERT(tm != NULL);
    TEST_ASSERT(cobalt_tstreemap_put(tm, "k", &v1) == 0);
    TEST_EQUAL(cobalt_tstreemap_size(tm), (size_t)1);
    cobalt_tstreemap_destroy(tm);

    printf("  Thread-safe wrapper allocator variants: OK\n");
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
    TEST_ASSERT(cobalt_tshashmap_put(NULL, NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tshashmap_get(NULL, "x") == NULL);
    TEST_ASSERT(cobalt_tshashmap_get(NULL, NULL) == NULL);
    TEST_ASSERT(cobalt_tshashmap_contains(NULL, "x") == 0);
    TEST_ASSERT(cobalt_tshashmap_contains(NULL, NULL) == 0);
    TEST_ASSERT(cobalt_tshashmap_remove(NULL, "x") == -1);
    TEST_ASSERT(cobalt_tshashmap_remove(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tshashmap_size(NULL) == 0);
    TEST_ASSERT(cobalt_tshashmap_capacity(NULL) == 0);
    cobalt_tshashmap_destroy(NULL); /* must not crash */
    printf("  Thread-safe hashmap NULL safety: OK\n");
}

void test_threadsafewrapper_hashmap_semantics(void)
{
    printf("Testing thread-safe hashmap key semantics...\n");
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(8);
    TEST_ASSERT(map != NULL);

    int v1 = 100, v2 = 200;
    TEST_ASSERT(cobalt_tshashmap_put(map, "key", &v1) == 0);
    TEST_ASSERT(cobalt_tshashmap_size(map) == 1);

    /* Absent key reads NULL */
    TEST_ASSERT(cobalt_tshashmap_get(map, "absent") == NULL);
    TEST_ASSERT(cobalt_tshashmap_contains(map, "absent") == 0);
    TEST_ASSERT(cobalt_tshashmap_remove(map, "absent") == -1);

    /* Same key overwrites in place: size stays 1, value updates */
    TEST_ASSERT(cobalt_tshashmap_put(map, "key", &v2) == 0);
    TEST_ASSERT(cobalt_tshashmap_size(map) == 1);
    TEST_ASSERT(*(int *)cobalt_tshashmap_get(map, "key") == 200);

    /* NULL key rejected on non-NULL map */
    TEST_ASSERT(cobalt_tshashmap_put(map, NULL, &v1) == -1);
    TEST_ASSERT(cobalt_tshashmap_contains(map, NULL) == 0);

    cobalt_tshashmap_destroy(map);
    printf("  Thread-safe hashmap key semantics: OK\n");
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
    TEST_ASSERT(cobalt_tslist_push_back(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tslist_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_tslist_pop_back(NULL) == NULL);
    TEST_ASSERT(cobalt_tslist_get(NULL, 0) == NULL);
    TEST_ASSERT(cobalt_tslist_remove_at(NULL, 0) == -1);
    TEST_ASSERT(cobalt_tslist_size(NULL) == 0);
    TEST_ASSERT(cobalt_tslist_is_empty(NULL) == 1);
    cobalt_tslist_destroy(NULL); /* must not crash */
    printf("  Thread-safe list NULL safety: OK\n");
}

void test_threadsafewrapper_list_remove_at(void)
{
    printf("Testing thread-safe list remove_at...\n");
    cobalt_tslist_t *list = cobalt_tslist_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tslist_push_back(list, &a) == 0);
    TEST_ASSERT(cobalt_tslist_push_back(list, &b) == 0);
    TEST_ASSERT(cobalt_tslist_push_back(list, &c) == 0);
    TEST_ASSERT(cobalt_tslist_size(list) == 3);

    /* Out-of-range removal fails */
    TEST_ASSERT(cobalt_tslist_remove_at(list, 3) == -1);
    TEST_ASSERT(cobalt_tslist_remove_at(list, 100) == -1);
    TEST_ASSERT(cobalt_tslist_size(list) == 3);

    /* In-range removal unlinks the node */
    TEST_ASSERT(cobalt_tslist_remove_at(list, 1) == 0);
    TEST_ASSERT(cobalt_tslist_size(list) == 2);
    TEST_ASSERT(*(int *)cobalt_tslist_get(list, 0) == 1);
    TEST_ASSERT(*(int *)cobalt_tslist_get(list, 1) == 3);
    TEST_ASSERT(cobalt_tslist_get(list, 2) == NULL);

    /* Drain to empty */
    TEST_ASSERT(cobalt_tslist_remove_at(list, 0) == 0);
    TEST_ASSERT(cobalt_tslist_remove_at(list, 0) == 0);
    TEST_ASSERT(cobalt_tslist_size(list) == 0);
    TEST_ASSERT(cobalt_tslist_is_empty(list) == 1);

    cobalt_tslist_destroy(list);
    printf("  Thread-safe list remove_at: OK\n");
}

void test_threadsafewrapper_deque_null(void)
{
    printf("Testing thread-safe deque NULL safety...\n");
    TEST_ASSERT(cobalt_tsdeque_push_front(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tsdeque_push_back(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tsdeque_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_tsdeque_pop_back(NULL) == NULL);
    TEST_ASSERT(cobalt_tsdeque_peek_front(NULL) == NULL);
    TEST_ASSERT(cobalt_tsdeque_peek_back(NULL) == NULL);
    TEST_ASSERT(cobalt_tsdeque_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsdeque_is_empty(NULL) == 1);
    cobalt_tsdeque_destroy(NULL); /* must not crash */
    printf("  Thread-safe deque NULL safety: OK\n");
}

void test_threadsafewrapper_deque_basic(void)
{
    printf("Testing thread-safe deque basic operations...\n");
    cobalt_tsdeque_t *dq = cobalt_tsdeque_create();
    TEST_ASSERT(dq != NULL);
    TEST_ASSERT(cobalt_tsdeque_is_empty(dq) == 1);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tsdeque_push_back(dq, &a) == 0);
    TEST_ASSERT(cobalt_tsdeque_push_back(dq, &b) == 0);
    TEST_ASSERT(cobalt_tsdeque_push_front(dq, &c) == 0);
    TEST_ASSERT(cobalt_tsdeque_size(dq) == 3);

    /* push_front put c at the head */
    TEST_ASSERT(cobalt_tsdeque_peek_front(dq) == &c);
    TEST_ASSERT(cobalt_tsdeque_peek_back(dq) == &b);

    TEST_ASSERT(cobalt_tsdeque_pop_front(dq) == &c);
    TEST_ASSERT(cobalt_tsdeque_size(dq) == 2);

    /* Peeking does not consume */
    TEST_ASSERT(cobalt_tsdeque_peek_front(dq) == &a);
    TEST_ASSERT(cobalt_tsdeque_size(dq) == 2);

    TEST_ASSERT(cobalt_tsdeque_pop_back(dq) == &b);
    TEST_ASSERT(cobalt_tsdeque_pop_front(dq) == &a);
    TEST_ASSERT(cobalt_tsdeque_size(dq) == 0);
    TEST_ASSERT(cobalt_tsdeque_is_empty(dq) == 1);

    cobalt_tsdeque_destroy(dq);
    printf("  Thread-safe deque basic: OK\n");
}

void test_threadsafewrapper_queue_null(void)
{
    printf("Testing thread-safe queue NULL safety...\n");
    TEST_ASSERT(cobalt_tsqueue_enqueue(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tsqueue_dequeue(NULL) == NULL);
    TEST_ASSERT(cobalt_tsqueue_peek(NULL) == NULL);
    TEST_ASSERT(cobalt_tsqueue_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsqueue_is_empty(NULL) == 1);
    cobalt_tsqueue_destroy(NULL); /* must not crash */
    printf("  Thread-safe queue NULL safety: OK\n");
}

void test_threadsafewrapper_queue_basic(void)
{
    printf("Testing thread-safe queue basic operations...\n");
    cobalt_tsqueue_t *q = cobalt_tsqueue_create();
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(cobalt_tsqueue_is_empty(q) == 1);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tsqueue_enqueue(q, &a) == 0);
    TEST_ASSERT(cobalt_tsqueue_enqueue(q, &b) == 0);
    TEST_ASSERT(cobalt_tsqueue_enqueue(q, &c) == 0);
    TEST_ASSERT(cobalt_tsqueue_size(q) == 3);
    TEST_ASSERT(cobalt_tsqueue_is_empty(q) == 0);

    /* FIFO order: peek sees the oldest without consuming */
    TEST_ASSERT(cobalt_tsqueue_peek(q) == &a);
    TEST_ASSERT(cobalt_tsqueue_size(q) == 3);
    TEST_ASSERT(cobalt_tsqueue_dequeue(q) == &a);
    TEST_ASSERT(cobalt_tsqueue_dequeue(q) == &b);
    TEST_ASSERT(cobalt_tsqueue_dequeue(q) == &c);
    TEST_ASSERT(cobalt_tsqueue_size(q) == 0);
    TEST_ASSERT(cobalt_tsqueue_is_empty(q) == 1);

    cobalt_tsqueue_destroy(q);
    printf("  Thread-safe queue basic: OK\n");
}

void test_threadsafewrapper_stack_null(void)
{
    printf("Testing thread-safe stack NULL safety...\n");
    TEST_ASSERT(cobalt_tsstack_push(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_tsstack_pop(NULL) == NULL);
    TEST_ASSERT(cobalt_tsstack_peek(NULL) == NULL);
    TEST_ASSERT(cobalt_tsstack_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsstack_is_empty(NULL) == 1);
    cobalt_tsstack_destroy(NULL); /* must not crash */
    printf("  Thread-safe stack NULL safety: OK\n");
}

void test_threadsafewrapper_stack_basic(void)
{
    printf("Testing thread-safe stack basic operations...\n");
    cobalt_tsstack_t *stk = cobalt_tsstack_create();
    TEST_ASSERT(stk != NULL);
    TEST_ASSERT(cobalt_tsstack_is_empty(stk) == 1);

    int a = 1, b = 2, c = 3;
    TEST_ASSERT(cobalt_tsstack_push(stk, &a) == 0);
    TEST_ASSERT(cobalt_tsstack_push(stk, &b) == 0);
    TEST_ASSERT(cobalt_tsstack_push(stk, &c) == 0);
    TEST_ASSERT(cobalt_tsstack_size(stk) == 3);
    TEST_ASSERT(cobalt_tsstack_is_empty(stk) == 0);

    /* LIFO order: peek sees the newest without consuming */
    TEST_ASSERT(cobalt_tsstack_peek(stk) == &c);
    TEST_ASSERT(cobalt_tsstack_size(stk) == 3);
    TEST_ASSERT(cobalt_tsstack_pop(stk) == &c);
    TEST_ASSERT(cobalt_tsstack_pop(stk) == &b);
    TEST_ASSERT(cobalt_tsstack_pop(stk) == &a);
    TEST_ASSERT(cobalt_tsstack_size(stk) == 0);
    TEST_ASSERT(cobalt_tsstack_is_empty(stk) == 1);

    cobalt_tsstack_destroy(stk);
    printf("  Thread-safe stack basic: OK\n");
}

void test_threadsafewrapper_set_null(void)
{
    printf("Testing thread-safe set NULL safety...\n");
    TEST_ASSERT(cobalt_tsset_insert(NULL, "x") == -1);
    TEST_ASSERT(cobalt_tsset_insert_ext(NULL, "x", 1) == -1);
    TEST_ASSERT(cobalt_tsset_contains(NULL, "x") == 0);
    TEST_ASSERT(cobalt_tsset_contains_ext(NULL, "x", 1) == 0);
    TEST_ASSERT(cobalt_tsset_remove(NULL, "x") == -1);
    TEST_ASSERT(cobalt_tsset_remove_ext(NULL, "x", 1) == -1);
    TEST_ASSERT(cobalt_tsset_size(NULL) == 0);
    TEST_ASSERT(cobalt_tsset_is_empty(NULL) == 1);
    cobalt_tsset_destroy(NULL); /* must not crash */
    printf("  Thread-safe set NULL safety: OK\n");
}

void test_threadsafewrapper_set_basic(void)
{
    printf("Testing thread-safe set basic operations...\n");
    cobalt_tsset_t *ss = cobalt_tsset_create(8);
    TEST_ASSERT(ss != NULL);
    TEST_ASSERT(cobalt_tsset_is_empty(ss) == 1);

    TEST_ASSERT(cobalt_tsset_insert(ss, "alpha") == 0);
    TEST_ASSERT(cobalt_tsset_insert(ss, "beta") == 0);
    TEST_ASSERT(cobalt_tsset_size(ss) == 2);
    TEST_ASSERT(cobalt_tsset_is_empty(ss) == 0);

    TEST_ASSERT(cobalt_tsset_contains(ss, "alpha") == 1);
    TEST_ASSERT(cobalt_tsset_contains(ss, "beta") == 1);
    TEST_ASSERT(cobalt_tsset_contains(ss, "gamma") == 0);

    /* Re-inserting an existing member is idempotent in size */
    TEST_ASSERT(cobalt_tsset_insert(ss, "alpha") == 0);
    TEST_ASSERT(cobalt_tsset_size(ss) == 2);

    /* Removing an absent member reports failure */
    TEST_ASSERT(cobalt_tsset_remove(ss, "gamma") == -1);
    TEST_ASSERT(cobalt_tsset_size(ss) == 2);

    TEST_ASSERT(cobalt_tsset_remove(ss, "alpha") == 0);
    TEST_ASSERT(cobalt_tsset_contains(ss, "alpha") == 0);
    TEST_ASSERT(cobalt_tsset_size(ss) == 1);

    /* Binary element variant round trips */
    TEST_ASSERT(cobalt_tsset_insert_ext(ss, "bin", 3) == 0);
    TEST_ASSERT(cobalt_tsset_contains_ext(ss, "bin", 3) == 1);
    TEST_ASSERT(cobalt_tsset_remove_ext(ss, "bin", 3) == 0);
    TEST_ASSERT(cobalt_tsset_contains_ext(ss, "bin", 3) == 0);

    cobalt_tsset_destroy(ss);
    printf("  Thread-safe set basic: OK\n");
}

void test_threadsafewrapper_treemap_null(void)
{
    printf("Testing thread-safe treemap NULL safety...\n");
    TEST_ASSERT(cobalt_tstreemap_put(NULL, "k", NULL) == -1);
    TEST_ASSERT(cobalt_tstreemap_get(NULL, "k") == NULL);
    TEST_ASSERT(cobalt_tstreemap_remove(NULL, "k") == -1);
    TEST_ASSERT(cobalt_tstreemap_min_key(NULL) == NULL);
    TEST_ASSERT(cobalt_tstreemap_max_key(NULL) == NULL);
    TEST_ASSERT(cobalt_tstreemap_size(NULL) == 0);
    cobalt_tstreemap_destroy(NULL); /* must not crash */
    printf("  Thread-safe treemap NULL safety: OK\n");
}

void test_threadsafewrapper_treemap_basic(void)
{
    printf("Testing thread-safe treemap basic operations...\n");
    cobalt_tstreemap_t *tm = cobalt_tstreemap_create();
    TEST_ASSERT(tm != NULL);
    TEST_ASSERT(cobalt_tstreemap_size(tm) == 0);

    int v1 = 10, v2 = 20, v3 = 30;
    TEST_ASSERT(cobalt_tstreemap_put(tm, "banana", &v1) == 0);
    TEST_ASSERT(cobalt_tstreemap_put(tm, "apple", &v2) == 0);
    TEST_ASSERT(cobalt_tstreemap_put(tm, "cherry", &v3) == 0);
    TEST_ASSERT(cobalt_tstreemap_size(tm) == 3);

    /* Missing key reads NULL, removal reports failure */
    TEST_ASSERT(cobalt_tstreemap_get(tm, "durian") == NULL);
    TEST_ASSERT(cobalt_tstreemap_remove(tm, "durian") == -1);

    /* Same key overwrites in place: size stays 3 */
    TEST_ASSERT(cobalt_tstreemap_put(tm, "apple", &v1) == 0);
    TEST_ASSERT(cobalt_tstreemap_size(tm) == 3);
    TEST_ASSERT(*(int *)cobalt_tstreemap_get(tm, "apple") == 10);

    /* Keys are ordered, so min/max expose lexical bounds */
    TEST_ASSERT(strcmp(cobalt_tstreemap_min_key(tm), "apple") == 0);
    TEST_ASSERT(strcmp(cobalt_tstreemap_max_key(tm), "cherry") == 0);

    /* NULL key rejected on non-NULL map */
    TEST_ASSERT(cobalt_tstreemap_put(tm, NULL, &v1) == -1);

    TEST_ASSERT(cobalt_tstreemap_remove(tm, "apple") == 0);
    TEST_ASSERT(cobalt_tstreemap_size(tm) == 2);
    TEST_ASSERT(strcmp(cobalt_tstreemap_min_key(tm), "banana") == 0);

    cobalt_tstreemap_destroy(tm);
    printf("  Thread-safe treemap basic: OK\n");
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
    test_threadsafewrapper_vector_bounds();
    test_threadsafewrapper_vector_allocator();
    test_threadsafewrapper_ts_allocator_variants();
    test_threadsafewrapper_hashmap_basic();
    test_threadsafewrapper_hashmap_null();
    test_threadsafewrapper_hashmap_semantics();
    test_threadsafewrapper_list_basic();
    test_threadsafewrapper_list_null();
    test_threadsafewrapper_list_remove_at();
    test_threadsafewrapper_deque_null();
    test_threadsafewrapper_deque_basic();
    test_threadsafewrapper_queue_null();
    test_threadsafewrapper_queue_basic();
    test_threadsafewrapper_stack_null();
    test_threadsafewrapper_stack_basic();
    test_threadsafewrapper_set_null();
    test_threadsafewrapper_set_basic();
    test_threadsafewrapper_treemap_null();
    test_threadsafewrapper_treemap_basic();
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
