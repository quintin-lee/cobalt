/**
 * @file test_lfhashmap.c
 * @brief Unit tests for Layer 4 Lock-Free Concurrent Extensible Hashmap
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/container/lfhashmap.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/thread.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_lfhashmap_create_destroy_invalid(void)
{
    printf("Testing lfhashmap create, destroy, and invalid arguments...\n");

    /* 1. Generic string hashmap */
    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create(NULL, NULL);
    TEST_ASSERT(map != NULL);
    TEST_EQUAL(cobalt_lfhashmap_size(map), 0);
    TEST_ASSERT(cobalt_lfhashmap_is_empty(map));
    TEST_ASSERT(cobalt_lfhashmap_bucket_count(map) >= 4);
    cobalt_lfhashmap_destroy(map);

    /* 2. Fast pointer hashmap */
    cobalt_lfhashmap_t *pmap = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(pmap != NULL);
    TEST_EQUAL(cobalt_lfhashmap_size(pmap), 0);
    cobalt_lfhashmap_destroy(pmap);

    /* 3. Custom allocator */
    cobalt_allocator_t *sys_alloc = cobalt_allocator_get_system();
    cobalt_lfhashmap_t *amap =
        cobalt_lfhashmap_create_with_allocator(NULL, NULL, sys_alloc, NULL, true);
    TEST_ASSERT(amap != NULL);
    cobalt_lfhashmap_destroy(amap);

    /* 4. Invalid arguments */
    TEST_ASSERT(cobalt_lfhashmap_insert(NULL, (void *)1, (void *)2) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_lfhashmap_insert(pmap, NULL, (void *)2) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_lfhashmap_get(NULL, (void *)1, NULL) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_lfhashmap_get(pmap, NULL, NULL) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(!cobalt_lfhashmap_contains(NULL, (void *)1));
    TEST_ASSERT(!cobalt_lfhashmap_contains(pmap, NULL));
    TEST_ASSERT(cobalt_lfhashmap_remove(NULL, (void *)1, NULL) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_lfhashmap_remove(pmap, NULL, NULL) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_EQUAL(cobalt_lfhashmap_size(NULL), 0);
    TEST_EQUAL(cobalt_lfhashmap_bucket_count(NULL), 0);
    TEST_ASSERT(cobalt_lfhashmap_is_empty(NULL));

    /* NULL destroy is a safe no-op */
    cobalt_lfhashmap_destroy(NULL);

    printf("  lfhashmap create/destroy: OK\n");
}

static void test_lfhashmap_insert_get_contains(void)
{
    printf("Testing lfhashmap insert, get, and contains...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    /* Insert 100 elements */
    for (intptr_t i = 1; i <= 100; i++) {
        int rc = cobalt_lfhashmap_insert(map, (void *)i, (void *)(i * 10));
        TEST_EQUAL(rc, 0);
    }

    TEST_EQUAL(cobalt_lfhashmap_size(map), 100);
    TEST_ASSERT(!cobalt_lfhashmap_is_empty(map));

    /* Lookup all 100 elements */
    for (intptr_t i = 1; i <= 100; i++) {
        void *val = NULL;
        int   rc  = cobalt_lfhashmap_get(map, (void *)i, &val);
        TEST_EQUAL(rc, 0);
        TEST_EQUAL((intptr_t)val, i * 10);
        TEST_ASSERT(cobalt_lfhashmap_contains(map, (void *)i));
    }

    /* Missing elements */
    for (intptr_t i = 101; i <= 150; i++) {
        void *val = NULL;
        int   rc  = cobalt_lfhashmap_get(map, (void *)i, &val);
        TEST_ASSERT(rc == COBALT_ERROR_NOT_FOUND);
        TEST_ASSERT(!cobalt_lfhashmap_contains(map, (void *)i));
    }

    /* Duplicate insert rejected */
    int rc = cobalt_lfhashmap_insert(map, (void *)42, (void *)9999);
    TEST_EQUAL(rc, COBALT_ERROR_ALREADY_EXISTS);

    void *val = NULL;
    cobalt_lfhashmap_get(map, (void *)42, &val);
    TEST_EQUAL((intptr_t)val, 420); /* Original value unmodified */

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap insert/get/contains: OK\n");
}

static void test_lfhashmap_put_upsert(void)
{
    printf("Testing lfhashmap put (upsert)...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    /* 1. Put new keys */
    void *old_val = (void *)0xdead;
    int   rc      = cobalt_lfhashmap_put(map, (void *)1, (void *)100, &old_val);
    TEST_EQUAL(rc, 0);
    TEST_ASSERT(old_val == NULL);
    TEST_EQUAL(cobalt_lfhashmap_size(map), 1);

    /* 2. Put existing key (update) */
    rc = cobalt_lfhashmap_put(map, (void *)1, (void *)200, &old_val);
    TEST_EQUAL(rc, 0);
    TEST_EQUAL((intptr_t)old_val, 100);
    TEST_EQUAL(cobalt_lfhashmap_size(map), 1);

    void *cur_val = NULL;
    cobalt_lfhashmap_get(map, (void *)1, &cur_val);
    TEST_EQUAL((intptr_t)cur_val, 200);

    /* 3. Put without out_old_value pointer */
    rc = cobalt_lfhashmap_put(map, (void *)1, (void *)300, NULL);
    TEST_EQUAL(rc, 0);
    cobalt_lfhashmap_get(map, (void *)1, &cur_val);
    TEST_EQUAL((intptr_t)cur_val, 300);

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap put: OK\n");
}

static void test_lfhashmap_remove(void)
{
    printf("Testing lfhashmap remove (two-phase deletion)...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    for (intptr_t i = 1; i <= 50; i++) {
        cobalt_lfhashmap_insert(map, (void *)i, (void *)(i * 5));
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), 50);

    /* Remove even keys */
    for (intptr_t i = 2; i <= 50; i += 2) {
        void *old_val = NULL;
        int   rc      = cobalt_lfhashmap_remove(map, (void *)i, &old_val);
        TEST_EQUAL(rc, 0);
        TEST_EQUAL((intptr_t)old_val, i * 5);
        TEST_ASSERT(!cobalt_lfhashmap_contains(map, (void *)i));
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), 25);

    /* Check odd keys still present */
    for (intptr_t i = 1; i <= 50; i += 2) {
        void *val = NULL;
        int   rc  = cobalt_lfhashmap_get(map, (void *)i, &val);
        TEST_EQUAL(rc, 0);
        TEST_EQUAL((intptr_t)val, i * 5);
    }

    /* Removing already removed key returns NOT_FOUND */
    TEST_ASSERT(cobalt_lfhashmap_remove(map, (void *)2, NULL) == COBALT_ERROR_NOT_FOUND);

    /* Remove remaining odd keys */
    for (intptr_t i = 1; i <= 50; i += 2) {
        TEST_EQUAL(cobalt_lfhashmap_remove(map, (void *)i, NULL), 0);
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), 0);
    TEST_ASSERT(cobalt_lfhashmap_is_empty(map));

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap remove: OK\n");
}

static void test_lfhashmap_string_keys(void)
{
    printf("Testing lfhashmap with generic string keys...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create(NULL, NULL);
    TEST_ASSERT(map != NULL);

    char keys[20][32];
    for (int i = 0; i < 20; i++) {
        snprintf(keys[i], sizeof(keys[i]), "user_key_%03d", i);
        TEST_EQUAL(cobalt_lfhashmap_insert(map, keys[i], (void *)(intptr_t)(i + 100)), 0);
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), 20);

    /* Lookup */
    for (int i = 0; i < 20; i++) {
        void *val = NULL;
        TEST_EQUAL(cobalt_lfhashmap_get(map, keys[i], &val), 0);
        TEST_EQUAL((intptr_t)val, i + 100);
    }

    /* Duplicate */
    TEST_EQUAL(cobalt_lfhashmap_insert(map, keys[0], (void *)999), COBALT_ERROR_ALREADY_EXISTS);

    /* Remove */
    for (int i = 0; i < 10; i++) {
        TEST_EQUAL(cobalt_lfhashmap_remove(map, keys[i], NULL), 0);
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), 10);

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(!cobalt_lfhashmap_contains(map, keys[i]));
    }
    for (int i = 10; i < 20; i++) {
        TEST_ASSERT(cobalt_lfhashmap_contains(map, keys[i]));
    }

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap string keys: OK\n");
}

static void test_lfhashmap_dynamic_resizing(void)
{
    printf("Testing lfhashmap dynamic zero-copy resizing (10,000 keys)...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    size_t init_buckets = cobalt_lfhashmap_bucket_count(map);
    TEST_EQUAL(init_buckets, 4);

    const int total = 10000;
    for (intptr_t i = 1; i <= total; i++) {
        int rc = cobalt_lfhashmap_insert(map, (void *)i, (void *)(i * 3));
        TEST_EQUAL(rc, 0);
    }

    TEST_EQUAL(cobalt_lfhashmap_size(map), (size_t)total);
    size_t expanded_buckets = cobalt_lfhashmap_bucket_count(map);
    /* 10,000 keys with 0.75 load factor -> bucket_count must have grown to at least 16384 */
    TEST_ASSERT(expanded_buckets >= 16384);

    /* Verify all 10,000 elements remain perfectly accessible */
    for (intptr_t i = 1; i <= total; i++) {
        void *val = NULL;
        int   rc  = cobalt_lfhashmap_get(map, (void *)i, &val);
        TEST_EQUAL(rc, 0);
        TEST_EQUAL((intptr_t)val, i * 3);
    }

    cobalt_lfhashmap_destroy(map);
    printf(
        "  lfhashmap dynamic resizing (4 -> %zu buckets, %d keys): OK\n", expanded_buckets, total);
}

static void test_lfhashmap_iterator(void)
{
    printf("Testing lfhashmap concurrent safe iterator...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    const int count = 200;
    for (intptr_t i = 1; i <= count; i++) {
        cobalt_lfhashmap_insert(map, (void *)i, (void *)(i * 7));
    }
    TEST_EQUAL(cobalt_lfhashmap_size(map), (size_t)count);

    cobalt_lfhashmap_iter_t *it = cobalt_lfhashmap_iter_create(map);
    TEST_ASSERT(it != NULL);

    bool  visited[201]  = {false};
    int   iterated_seen = 0;
    void *k = NULL, *v = NULL;

    while (cobalt_lfhashmap_iter_next(it, &k, &v)) {
        intptr_t key_val = (intptr_t)k;
        intptr_t val_val = (intptr_t)v;
        TEST_ASSERT(key_val >= 1 && key_val <= count);
        TEST_EQUAL(val_val, key_val * 7);
        TEST_ASSERT(!visited[key_val]); /* No duplicates */
        visited[key_val] = true;
        iterated_seen++;
    }

    TEST_EQUAL(iterated_seen, count);
    cobalt_lfhashmap_iter_destroy(it);

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap iterator: OK\n");
}

static void test_lfhashmap_pin_unpin(void)
{
    printf("Testing lfhashmap explicit pin/unpin guard...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    cobalt_lfhashmap_guard_t g = cobalt_lfhashmap_pin(map);
    TEST_ASSERT(g.handle != NULL);

    /* Operations under pinned epoch */
    cobalt_lfhashmap_insert(map, (void *)1, (void *)10);
    cobalt_lfhashmap_insert(map, (void *)2, (void *)20);
    TEST_ASSERT(cobalt_lfhashmap_contains(map, (void *)1));

    cobalt_lfhashmap_unpin(&g);
    TEST_ASSERT(g.handle == NULL);

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap pin/unpin: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Multithreaded Concurrency Stress Test                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    cobalt_lfhashmap_t *map;
    int                 thread_id;
    int                 num_ops;
    atomic_size_t      *reads;
    atomic_size_t      *writes;
    atomic_size_t      *removes;
} lfhashmap_stress_arg_t;

static void *lfhashmap_stress_worker(void *arg)
{
    lfhashmap_stress_arg_t *w    = (lfhashmap_stress_arg_t *)arg;
    uint32_t                seed = (uint32_t)w->thread_id + 1;

    for (int i = 0; i < w->num_ops; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        int op  = seed % 100;
        int key = (int)((seed >> 8) % 2000) + 1;

        if (op < 70) {
            void *val = NULL;
            cobalt_lfhashmap_get(w->map, (void *)(intptr_t)key, &val);
            atomic_fetch_add(w->reads, 1);
        } else if (op < 90) {
            cobalt_lfhashmap_put(w->map, (void *)(intptr_t)key, (void *)(intptr_t)(key * 2), NULL);
            atomic_fetch_add(w->writes, 1);
        } else {
            cobalt_lfhashmap_remove(w->map, (void *)(intptr_t)key, NULL);
            atomic_fetch_add(w->removes, 1);
        }
    }
    return NULL;
}

static void test_lfhashmap_concurrency_stress(void)
{
    printf("Testing lfhashmap 4-thread concurrency stress (mixed read/write/remove)...\n");

    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    TEST_ASSERT(map != NULL);

    /* Pre-populate 1000 items */
    for (int i = 1; i <= 1000; i++) {
        cobalt_lfhashmap_insert(map, (void *)(intptr_t)i, (void *)(intptr_t)(i * 2));
    }

    const int              num_threads    = 4;
    const int              ops_per_thread = 25000;
    cobalt_thread_t        threads[4];
    lfhashmap_stress_arg_t args[4];
    atomic_size_t          reads   = 0;
    atomic_size_t          writes  = 0;
    atomic_size_t          removes = 0;

    for (int i = 0; i < num_threads; i++) {
        args[i].map       = map;
        args[i].thread_id = i;
        args[i].num_ops   = ops_per_thread;
        args[i].reads     = &reads;
        args[i].writes    = &writes;
        args[i].removes   = &removes;
        TEST_EQUAL(cobalt_thread_create(lfhashmap_stress_worker, &args[i], &threads[i]), 0);
    }

    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }

    int total_done = (int)(atomic_load(&reads) + atomic_load(&writes) + atomic_load(&removes));
    TEST_EQUAL(total_done, num_threads * ops_per_thread);

    cobalt_lfhashmap_destroy(map);
    printf("  lfhashmap 4-thread concurrency stress (%d ops total): OK\n",
           num_threads * ops_per_thread);
}

void test_lfhashmap(void)
{
    test_lfhashmap_create_destroy_invalid();
    test_lfhashmap_insert_get_contains();
    test_lfhashmap_put_upsert();
    test_lfhashmap_remove();
    test_lfhashmap_string_keys();
    test_lfhashmap_dynamic_resizing();
    test_lfhashmap_iterator();
    test_lfhashmap_pin_unpin();
    test_lfhashmap_concurrency_stress();
}
