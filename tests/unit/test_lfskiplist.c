/**
 * @file test_lfskiplist.c
 * @brief Unit tests for Layer 4 Lock-Free Concurrent SkipList
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/algorithm/functional.h"
#include "cobalt/container/lfskiplist.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int int_comparator(const void *a, const void *b)
{
    intptr_t ia = (intptr_t)a;
    intptr_t ib = (intptr_t)b;
    if (ia < ib) {
        return -1;
    }
    if (ia > ib) {
        return 1;
    }
    return 0;
}

static int string_comparator(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

typedef struct {
    uint64_t high;
    uint64_t low;
} test_u128_key_t;

static int u128_comparator(const void *a, const void *b)
{
    const test_u128_key_t *ka = (const test_u128_key_t *)a;
    const test_u128_key_t *kb = (const test_u128_key_t *)b;
    if (ka->high < kb->high) {
        return -1;
    }
    if (ka->high > kb->high) {
        return 1;
    }
    if (ka->low < kb->low) {
        return -1;
    }
    if (ka->low > kb->low) {
        return 1;
    }
    return 0;
}

static void test_lfskiplist_create_destroy(void)
{
    printf("Testing lfskiplist create and destroy...\n");

    /* Null comparator should fail */
    TEST_ASSERT(cobalt_lfskiplist_create(NULL) == NULL);

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);
    TEST_ASSERT(cobalt_lfskiplist_is_empty(sl));
    TEST_EQUAL(cobalt_lfskiplist_size(sl), 0U);

    cobalt_lfskiplist_destroy(sl);
    cobalt_lfskiplist_destroy(NULL); /* Should not crash */

    /* Custom allocator and external shared EBR */
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    cobalt_ebr_t       *ebr   = cobalt_ebr_create(alloc);
    TEST_ASSERT(ebr != NULL);

    cobalt_lfskiplist_t *sl_custom =
        cobalt_lfskiplist_create_with_allocator(int_comparator, alloc, ebr);
    TEST_ASSERT(sl_custom != NULL);
    cobalt_lfskiplist_destroy(sl_custom);

    cobalt_ebr_destroy(ebr);
    printf("  lfskiplist create/destroy: OK\n");
}

static void test_lfskiplist_insert_get_contains(void)
{
    printf("Testing lfskiplist insert, get, and contains...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    const int count = 1000;

    /* Insert 1,000 keys */
    for (int i = 1; i <= count; i++) {
        void *key = (void *)(intptr_t)i;
        void *val = (void *)(intptr_t)(i * 10);
        TEST_EQUAL(cobalt_lfskiplist_insert(sl, key, val), 0);
    }

    TEST_EQUAL(cobalt_lfskiplist_size(sl), (size_t)count);
    TEST_ASSERT(!cobalt_lfskiplist_is_empty(sl));

    /* Verify all keys exist with correct values */
    for (int i = 1; i <= count; i++) {
        void *key     = (void *)(intptr_t)i;
        void *out_val = NULL;
        TEST_ASSERT(cobalt_lfskiplist_contains(sl, key));
        TEST_EQUAL(cobalt_lfskiplist_get(sl, key, &out_val), 0);
        TEST_EQUAL((intptr_t)out_val, (intptr_t)(i * 10));
    }

    /* Verify non-existent keys */
    TEST_ASSERT(!cobalt_lfskiplist_contains(sl, (void *)(intptr_t)0));
    TEST_ASSERT(!cobalt_lfskiplist_contains(sl, (void *)(intptr_t)(count + 1)));
    TEST_EQUAL(cobalt_lfskiplist_get(sl, (void *)(intptr_t)(count + 1), NULL), -1);

    /* Duplicate insert should return -1 */
    TEST_EQUAL(cobalt_lfskiplist_insert(sl, (void *)(intptr_t)500, (void *)(intptr_t)9999), -1);
    TEST_EQUAL(cobalt_lfskiplist_size(sl), (size_t)count);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist insert/get/contains: OK\n");
}

static void test_lfskiplist_put(void)
{
    printf("Testing lfskiplist put (upsert)...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    void *old_val = NULL;

    /* Put new key -> returns 0 */
    TEST_EQUAL(cobalt_lfskiplist_put(sl, (void *)(intptr_t)42, (void *)(intptr_t)100, &old_val), 0);
    TEST_EQUAL((intptr_t)old_val, 0);
    TEST_EQUAL(cobalt_lfskiplist_size(sl), 1U);

    /* Put existing key -> returns 1 and outputs old value */
    TEST_EQUAL(cobalt_lfskiplist_put(sl, (void *)(intptr_t)42, (void *)(intptr_t)200, &old_val), 1);
    TEST_EQUAL((intptr_t)old_val, 100);
    TEST_EQUAL(cobalt_lfskiplist_size(sl), 1U);

    /* Verify updated value */
    void *cur_val = NULL;
    TEST_EQUAL(cobalt_lfskiplist_get(sl, (void *)(intptr_t)42, &cur_val), 0);
    TEST_EQUAL((intptr_t)cur_val, 200);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist put: OK\n");
}

static void test_lfskiplist_remove(void)
{
    printf("Testing lfskiplist remove (two-phase deletion)...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    const int total = 1000;
    for (int i = 1; i <= total; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)i, (void *)(intptr_t)(i * 10));
    }

    /* Remove all even keys */
    for (int i = 2; i <= total; i += 2) {
        void *out_val = NULL;
        TEST_EQUAL(cobalt_lfskiplist_remove(sl, (void *)(intptr_t)i, &out_val), 0);
        TEST_EQUAL((intptr_t)out_val, (intptr_t)(i * 10));
    }

    TEST_EQUAL(cobalt_lfskiplist_size(sl), (size_t)(total / 2));

    /* Verify even keys are gone */
    for (int i = 2; i <= total; i += 2) {
        TEST_ASSERT(!cobalt_lfskiplist_contains(sl, (void *)(intptr_t)i));
        TEST_EQUAL(cobalt_lfskiplist_get(sl, (void *)(intptr_t)i, NULL), -1);
    }

    /* Verify odd keys are still present */
    for (int i = 1; i <= total; i += 2) {
        void *val = NULL;
        TEST_ASSERT(cobalt_lfskiplist_contains(sl, (void *)(intptr_t)i));
        TEST_EQUAL(cobalt_lfskiplist_get(sl, (void *)(intptr_t)i, &val), 0);
        TEST_EQUAL((intptr_t)val, (intptr_t)(i * 10));
    }

    /* Removing non-existent or already removed key should return -1 */
    TEST_EQUAL(cobalt_lfskiplist_remove(sl, (void *)(intptr_t)2, NULL), -1);
    TEST_EQUAL(cobalt_lfskiplist_remove(sl, (void *)(intptr_t)9999, NULL), -1);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist remove: OK\n");
}

static void test_lfskiplist_struct_keys(void)
{
    printf("Testing lfskiplist with string keys and struct keys...\n");

    /* String keys test */
    cobalt_lfskiplist_t *sl_str = cobalt_lfskiplist_create(string_comparator);
    TEST_ASSERT(sl_str != NULL);

    char *fruits[] = {"banana", "apple", "cherry", "date", "fig", "grape", "elderberry"};
    int   n_fruits = sizeof(fruits) / sizeof(fruits[0]);

    for (int i = 0; i < n_fruits; i++) {
        TEST_EQUAL(cobalt_lfskiplist_insert(sl_str, fruits[i], (void *)(intptr_t)i), 0);
    }

    TEST_EQUAL(cobalt_lfskiplist_size(sl_str), (size_t)n_fruits);

    for (int i = 0; i < n_fruits; i++) {
        void *v = NULL;
        TEST_ASSERT(cobalt_lfskiplist_contains(sl_str, fruits[i]));
        TEST_EQUAL(cobalt_lfskiplist_get(sl_str, fruits[i], &v), 0);
        TEST_EQUAL((intptr_t)v, (intptr_t)i);
    }

    cobalt_lfskiplist_destroy(sl_str);

    /* 128-bit struct keys test */
    cobalt_lfskiplist_t *sl_u128 = cobalt_lfskiplist_create(u128_comparator);
    TEST_ASSERT(sl_u128 != NULL);

    test_u128_key_t keys[10];
    for (int i = 0; i < 10; i++) {
        keys[i].high = (uint64_t)i * 1000;
        keys[i].low  = (uint64_t)i;
        TEST_EQUAL(cobalt_lfskiplist_insert(sl_u128, &keys[i], (void *)(intptr_t)i), 0);
    }

    TEST_EQUAL(cobalt_lfskiplist_size(sl_u128), 10U);

    for (int i = 0; i < 10; i++) {
        void *v = NULL;
        TEST_ASSERT(cobalt_lfskiplist_contains(sl_u128, &keys[i]));
        TEST_EQUAL(cobalt_lfskiplist_get(sl_u128, &keys[i], &v), 0);
        TEST_EQUAL((intptr_t)v, (intptr_t)i);
    }

    cobalt_lfskiplist_destroy(sl_u128);
    printf("  lfskiplist struct/string keys: OK\n");
}

static void test_lfskiplist_range_scan(void)
{
    printf("Testing lfskiplist range scan and iterator...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    /* Insert 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 */
    for (int i = 1; i <= 10; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)(i * 10), (void *)(intptr_t)(i * 10));
    }

    /* 1. Full unbounded scan [NULL, NULL] */
    cobalt_lfskiplist_iter_t *it = cobalt_lfskiplist_iter_create(sl, NULL, NULL);
    TEST_ASSERT(it != NULL);

    int   count  = 0;
    int   prev_k = 0;
    void *k = NULL, *v = NULL;

    while (cobalt_lfskiplist_iter_next(it, &k, &v)) {
        int key_val = (int)(intptr_t)k;
        TEST_ASSERT(key_val > prev_k);
        TEST_EQUAL((intptr_t)v, (intptr_t)key_val);
        prev_k = key_val;
        count++;
    }
    TEST_EQUAL(count, 10);
    cobalt_lfskiplist_iter_destroy(it);

    /* 2. Bounded scan [30, 70] */
    it = cobalt_lfskiplist_iter_create(sl, (void *)(intptr_t)30, (void *)(intptr_t)70);
    TEST_ASSERT(it != NULL);
    count = 0;
    while (cobalt_lfskiplist_iter_next(it, &k, &v)) {
        int key_val = (int)(intptr_t)k;
        TEST_ASSERT(key_val >= 30 && key_val <= 70);
        count++;
    }
    TEST_EQUAL(count, 5); /* 30, 40, 50, 60, 70 */
    cobalt_lfskiplist_iter_destroy(it);

    /* 3. Half-open [60, NULL] */
    it = cobalt_lfskiplist_iter_create(sl, (void *)(intptr_t)60, NULL);
    TEST_ASSERT(it != NULL);
    count = 0;
    while (cobalt_lfskiplist_iter_next(it, &k, &v)) {
        int key_val = (int)(intptr_t)k;
        TEST_ASSERT(key_val >= 60);
        count++;
    }
    TEST_EQUAL(count, 5); /* 60, 70, 80, 90, 100 */
    cobalt_lfskiplist_iter_destroy(it);

    /* 4. Inverted range [70, 30] -> empty */
    it = cobalt_lfskiplist_iter_create(sl, (void *)(intptr_t)70, (void *)(intptr_t)30);
    TEST_ASSERT(it != NULL);
    TEST_ASSERT(!cobalt_lfskiplist_iter_next(it, &k, &v));
    cobalt_lfskiplist_iter_destroy(it);

    /* 5. Range scan with concurrent insert */
    it = cobalt_lfskiplist_iter_create(sl, (void *)(intptr_t)25, (void *)(intptr_t)85);
    TEST_ASSERT(it != NULL);
    /* Concurrently insert 27 and 82 */
    cobalt_lfskiplist_insert(sl, (void *)(intptr_t)27, (void *)(intptr_t)27);
    cobalt_lfskiplist_insert(sl, (void *)(intptr_t)82, (void *)(intptr_t)82);

    count = 0;
    while (cobalt_lfskiplist_iter_next(it, &k, &v)) {
        count++;
    }
    TEST_ASSERT(count >= 6); /* Successfully iterated through without crashing */
    cobalt_lfskiplist_iter_destroy(it);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist range scan/iterator: OK\n");
}

static void test_lfskiplist_pin_unpin(void)
{
    printf("Testing lfskiplist explicit pin/unpin guard...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    cobalt_lfskiplist_guard_t g = cobalt_lfskiplist_pin(sl);
    TEST_ASSERT(g.handle != NULL);

    /* Perform operations under pinned guard */
    cobalt_lfskiplist_insert(sl, (void *)(intptr_t)1, (void *)(intptr_t)10);
    cobalt_lfskiplist_insert(sl, (void *)(intptr_t)2, (void *)(intptr_t)20);
    TEST_ASSERT(cobalt_lfskiplist_contains(sl, (void *)(intptr_t)1));

    cobalt_lfskiplist_unpin(&g);
    TEST_ASSERT(g.handle == NULL);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist explicit pin/unpin: OK\n");
}

typedef struct {
    cobalt_lfskiplist_t *sl;
    int                  thread_id;
    int                  num_ops;
    atomic_size_t       *reads;
    atomic_size_t       *writes;
    atomic_size_t       *removes;
} lfskiplist_stress_arg_t;

static void *lfskiplist_stress_worker(void *arg)
{
    lfskiplist_stress_arg_t *w    = (lfskiplist_stress_arg_t *)arg;
    uint32_t                 seed = (uint32_t)w->thread_id + 1;

    for (int i = 0; i < w->num_ops; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        int op  = seed % 100;
        int key = (seed >> 8) % 2000;

        if (op < 70) {
            void *val = NULL;
            cobalt_lfskiplist_get(w->sl, (void *)(intptr_t)key, &val);
            atomic_fetch_add(w->reads, 1);
        } else if (op < 90) {
            cobalt_lfskiplist_put(w->sl, (void *)(intptr_t)key, (void *)(intptr_t)(key * 2), NULL);
            atomic_fetch_add(w->writes, 1);
        } else {
            cobalt_lfskiplist_remove(w->sl, (void *)(intptr_t)key, NULL);
            atomic_fetch_add(w->removes, 1);
        }
    }
    return NULL;
}

static void test_lfskiplist_concurrency_stress(void)
{
    printf("Testing lfskiplist 4-thread concurrency stress (mixed read/write/remove)...\n");

    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    TEST_ASSERT(sl != NULL);

    for (int i = 0; i < 1000; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)i, (void *)(intptr_t)(i * 2));
    }

    const int               num_threads    = 4;
    const int               ops_per_thread = 25000;
    cobalt_thread_t         threads[4];
    lfskiplist_stress_arg_t args[4];
    atomic_size_t           reads   = 0;
    atomic_size_t           writes  = 0;
    atomic_size_t           removes = 0;

    for (int i = 0; i < num_threads; i++) {
        args[i].sl        = sl;
        args[i].thread_id = i;
        args[i].num_ops   = ops_per_thread;
        args[i].reads     = &reads;
        args[i].writes    = &writes;
        args[i].removes   = &removes;
        TEST_EQUAL(cobalt_thread_create(lfskiplist_stress_worker, &args[i], &threads[i]), 0);
    }

    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }

    int total_done = (int)(atomic_load(&reads) + atomic_load(&writes) + atomic_load(&removes));
    TEST_EQUAL(total_done, num_threads * ops_per_thread);

    cobalt_lfskiplist_destroy(sl);
    printf("  lfskiplist 4-thread concurrency stress (%d ops total): OK\n",
           num_threads * ops_per_thread);
}

void test_lfskiplist(void)
{
    test_lfskiplist_create_destroy();
    test_lfskiplist_insert_get_contains();
    test_lfskiplist_put();
    test_lfskiplist_remove();
    test_lfskiplist_struct_keys();
    test_lfskiplist_range_scan();
    test_lfskiplist_pin_unpin();
    test_lfskiplist_concurrency_stress();
}
