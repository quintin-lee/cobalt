/**
 * @file test_hashmap.c
 * @Unit test for hash map container.
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/container/set.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_hashmap_basic(void)
{
    printf("Testing hashmap basic operations...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    if (!map) {
        fprintf(stderr, "ERROR: Failed to create hashmap\n");
        return;
    }
    printf("  Created hashmap\n");

    /* Use static values to avoid heap issues */
    static int val1 = 100;
    static int val2 = 200;

    int ret = cobalt_hashmap_put(map, "name", &val1);
    printf("  Put 'name': %s\n", ret == 0 ? "OK" : "FAIL");

    ret = cobalt_hashmap_put(map, "age", &val2);
    printf("  Put 'age': %s\n", ret == 0 ? "OK" : "FAIL");

    int *got = (int *)cobalt_hashmap_get(map, "name");
    if (got && *got == 100) {
        printf("  Get 'name' returns 100: OK\n");
    } else {
        printf("  Get 'name': FAIL\n");
    }

    size_t sz = cobalt_hashmap_size(map);
    printf("  Size is %zu\n", sz);

    cobalt_hashmap_destroy(map);
    printf("  Hashmap tests completed\n");
}

void test_hashmap_resize(void)
{
    printf("Testing hashmap resize...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(8);
    TEST_ASSERT(map != NULL);

    TEST_ASSERT(cobalt_hashmap_capacity(map) == 8);

    int  values[100];
    char keys[100][16];
    for (int i = 0; i < 100; i++) {
        snprintf(keys[i], 16, "key_%d", i);
        values[i] = i * 10;
        TEST_ASSERT(cobalt_hashmap_put(map, keys[i], &values[i]) == 0);
    }

    TEST_ASSERT(cobalt_hashmap_size(map) == 100);
    TEST_ASSERT(cobalt_hashmap_capacity(map) > 8);

    for (int i = 0; i < 100; i++) {
        void *val = cobalt_hashmap_get(map, keys[i]);
        TEST_ASSERT(val != NULL);
        TEST_ASSERT(*(int *)val == i * 10);
    }

    cobalt_hashmap_destroy(map);
    printf("  Hashmap resize test passed\n");
}

void test_hashmap_zero_initial_capacity(void)
{
    printf("Testing hashmap zero initial capacity...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(0);
    TEST_ASSERT(map != NULL);
    TEST_ASSERT(cobalt_hashmap_size(map) == 0);
    TEST_ASSERT(cobalt_hashmap_capacity(map) == 0);

    int value = 42;
    TEST_ASSERT(cobalt_hashmap_put(map, "first", &value) == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 1);
    TEST_ASSERT(cobalt_hashmap_capacity(map) > 0);
    TEST_ASSERT(*(int *)cobalt_hashmap_get(map, "first") == 42);

    cobalt_hashmap_destroy(map);
    printf("  Hashmap zero initial capacity test passed\n");
}

void test_hashmap_resize_stress(void)
{
    printf("Testing hashmap resize stress...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);

    size_t initial_capacity = cobalt_hashmap_capacity(map);
    TEST_ASSERT(initial_capacity == 4);

    /* Insert enough to trigger multiple resizes */
    int  values[200];
    char keys[200][32];

    for (int i = 0; i < 200; i++) {
        snprintf(keys[i], 32, "stress_key_%d", i);
        values[i] = i * 100;
        TEST_ASSERT(cobalt_hashmap_put(map, keys[i], &values[i]) == 0);
    }

    TEST_ASSERT(cobalt_hashmap_size(map) == 200);
    TEST_ASSERT(cobalt_hashmap_capacity(map) > initial_capacity);

    /* Verify all values still accessible after resizes */
    for (int i = 0; i < 200; i++) {
        void *val = cobalt_hashmap_get(map, keys[i]);
        TEST_ASSERT(val != NULL);
        TEST_ASSERT(*(int *)val == i * 100);
    }

    cobalt_hashmap_destroy(map);
    printf("  Resize stress test passed\n");
}

void test_hashmap_empty_operations(void);
void test_hashmap_single_element(void);
void test_hashmap_null_key(void);
void test_hashmap_overwrite_then_remove(void);

static unsigned int hash_always_zero(const void *key, size_t key_len)
{
    (void)key;
    (void)key_len;
    return 0u;
}

static int equal_strcmp2(const void *a, const void *b, size_t len)
{
    (void)len;
    return strcmp((const char *)a, (const char *)b) == 0;
}

void test_hashmap_collision_stress(void);
void test_hashmap_set_funcs(void);

static unsigned int hash_fnv1a(const void *key, size_t key_len)
{
    (void)key_len;
    unsigned int h = 2166136261u;
    const char  *k = (const char *)key;
    while (*k) {
        h = h * 31 + (unsigned char)*k++;
    }
    return h;
}

static int equal_strcmp(const void *a, const void *b, size_t len)
{
    (void)len;
    return strcmp((const char *)a, (const char *)b) == 0;
}

void test_hashmap_set_funcs(void)
{
    printf("Testing hashmap set_funcs...\n");

    /* Create with default functions */
    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);

    int ret = cobalt_hashmap_put(map, "name", "Alice");
    TEST_ASSERT(ret == 0);

    const char *val = (const char *)cobalt_hashmap_get(map, "name");
    TEST_ASSERT(val != NULL);
    TEST_ASSERT(strcmp(val, "Alice") == 0);

    /* Change hash function to a custom one */
    ret = cobalt_hashmap_set_funcs(map, hash_fnv1a, equal_strcmp);
    TEST_ASSERT(ret == 0);

    /* Re-insert with new hash function — existing entries may not be
     * findable since they were hashed with the previous function */
    ret = cobalt_hashmap_put(map, "age", "30");
    TEST_ASSERT(ret == 0);

    val = (const char *)cobalt_hashmap_get(map, "age");
    TEST_ASSERT(val != NULL && strcmp(val, "30") == 0);

    /* Verify old key is gone (different hash) */
    val = (const char *)cobalt_hashmap_get(map, "name");
    TEST_ASSERT(val == NULL);

    cobalt_hashmap_destroy(map);
    printf("  set_funcs: OK\n");
}


void test_hashmap(void)
{
    printf("Testing hashmap...\n");
    test_hashmap_basic();
    test_hashmap_resize();
    test_hashmap_zero_initial_capacity();
    test_hashmap_resize_stress();
    test_hashmap_empty_operations();
    test_hashmap_single_element();
    test_hashmap_null_key();
    test_hashmap_overwrite_then_remove();
    test_hashmap_collision_stress();
    test_hashmap_set_funcs();
    printf("  Hashmap tests completed\n");
}

/* -------------------------------------------------------------------------- */
/* Generic (ext) API tests                                                    */
/* -------------------------------------------------------------------------- */

/* Djb2 hash for int keys */
static unsigned int hash_int(const void *key, size_t key_len)
{
    (void)key_len;
    const int   *k = (const int *)key;
    unsigned int h = 5381;
    for (size_t i = 0; i < sizeof(int); i++) {
        h = h * 33 + ((const unsigned char *)k)[i];
    }
    return h;
}

static int equal_int(const void *a, const void *b, size_t key_len)
{
    (void)key_len;
    return *(const int *)a == *(const int *)b;
}

void test_hashmap_ext_int_keys(void)
{
    printf("Testing hashmap ext with int keys...\n");
    cobalt_hashmap_t *map = cobalt_hashmap_create_ext(8, hash_int, equal_int);
    TEST_ASSERT(map != NULL);

    int        key1 = 42, key2 = 99;
    static int val_a = 100, val_b = 200;

    TEST_ASSERT(cobalt_hashmap_put_ext(map, &key1, sizeof(int), &val_a) == 0);
    TEST_ASSERT(cobalt_hashmap_put_ext(map, &key2, sizeof(int), &val_b) == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 2);

    int *got = (int *)cobalt_hashmap_get_ext(map, &key1, sizeof(int));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 100);

    got = (int *)cobalt_hashmap_get_ext(map, &key2, sizeof(int));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 200);

    /* Update existing key */
    static int val_a2 = 999;
    TEST_ASSERT(cobalt_hashmap_put_ext(map, &key1, sizeof(int), &val_a2) == 0);
    got = (int *)cobalt_hashmap_get_ext(map, &key1, sizeof(int));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 999);

    /* Remove */
    TEST_ASSERT(cobalt_hashmap_remove_ext(map, &key1, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 1);
    TEST_ASSERT(cobalt_hashmap_get_ext(map, &key1, sizeof(int)) == NULL);

    cobalt_hashmap_destroy(map);
    printf("  hashmap ext int keys: OK\n");
}

void test_hashmap_ext_null_callbacks(void)
{
    printf("Testing hashmap ext with NULL callbacks (string mode)...\n");
    cobalt_hashmap_t *map = cobalt_hashmap_create_ext(8, NULL, NULL);
    TEST_ASSERT(map != NULL);

    static int val = 77;
    TEST_ASSERT(cobalt_hashmap_put(map, "hello", &val) == 0);
    int *got = (int *)cobalt_hashmap_get(map, "hello");
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 77);

    cobalt_hashmap_destroy(map);
    printf("  hashmap ext NULL callbacks: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Boundary condition tests                                                    */
/* -------------------------------------------------------------------------- */

void test_hashmap_empty_operations(void)
{
    printf("Testing hashmap empty map operations...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);
    TEST_ASSERT(cobalt_hashmap_size(map) == 0);
    TEST_ASSERT(cobalt_hashmap_get(map, "nonexistent") == NULL);
    TEST_ASSERT(cobalt_hashmap_remove(map, "nonexistent") == -1);
    cobalt_map_iterator_t *it = cobalt_hashmap_iterator_create(map);
    TEST_ASSERT(it != NULL);
    cobalt_map_pair_t pair = cobalt_map_iterator_next(it);
    TEST_ASSERT(pair.key == NULL && pair.value == NULL);
    cobalt_map_iterator_destroy(it);
    cobalt_hashmap_destroy(map);
    printf("  Empty map operations: OK\n");
}

void test_hashmap_single_element(void)
{
    printf("Testing hashmap single element...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);

    static int val = 42;
    TEST_ASSERT(cobalt_hashmap_put(map, "solo", &val) == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 1);
    TEST_ASSERT(cobalt_hashmap_get(map, "solo") == &val);
    TEST_ASSERT(cobalt_hashmap_get(map, "other") == NULL);
    TEST_ASSERT(cobalt_hashmap_remove(map, "solo") == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 0);
    TEST_ASSERT(cobalt_hashmap_get(map, "solo") == NULL);
    cobalt_hashmap_destroy(map);
    printf("  Single element: OK\n");
}

void test_hashmap_null_key(void)
{
    printf("Testing hashmap null key handling...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);

    int val = 1;
    TEST_ASSERT(cobalt_hashmap_put(map, NULL, &val) == -1);
    TEST_ASSERT(cobalt_hashmap_get(map, NULL) == NULL);
    TEST_ASSERT(cobalt_hashmap_remove(map, NULL) == -1);
    cobalt_hashmap_destroy(map);
    printf("  Null key handling: OK\n");
}

void test_hashmap_overwrite_then_remove(void)
{
    printf("Testing hashmap overwrite and remove cycle...\n");

    cobalt_hashmap_t *map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);

    static int v1 = 1, v2 = 2, v3 = 3;
    TEST_ASSERT(cobalt_hashmap_put(map, "k", &v1) == 0);
    TEST_ASSERT(cobalt_hashmap_put(map, "k", &v2) == 0);
    TEST_ASSERT(cobalt_hashmap_put(map, "k", &v3) == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 1);
    TEST_ASSERT(*(int *)cobalt_hashmap_get(map, "k") == 3);
    TEST_ASSERT(cobalt_hashmap_remove(map, "k") == 0);
    TEST_ASSERT(cobalt_hashmap_size(map) == 0);
    cobalt_hashmap_destroy(map);
    printf("  Overwrite and remove cycle: OK\n");
}

void test_hashmap_collision_stress(void)
{
    printf("Testing hashmap collision stress...\n");

    /* Custom hash function that maps all keys to the same bucket (hash = 0) */
    cobalt_hashmap_t *map = cobalt_hashmap_create_ext(4, hash_always_zero, equal_strcmp2);
    TEST_ASSERT(map != NULL);

    /* Insert many keys that all hash to 0 — tests separate chaining under pressure */
    int  values[256];
    char keys[256][32];
    for (int i = 0; i < 256; i++) {
        snprintf(keys[i], 32, "collision_key_%d", i);
        values[i] = i * 100;
        int ret   = cobalt_hashmap_put(map, keys[i], &values[i]);
        TEST_ASSERT(ret == 0);
    }

    TEST_ASSERT(cobalt_hashmap_size(map) == 256);

    /* Verify all values are retrievable */
    for (int i = 0; i < 256; i++) {
        int *got = (int *)cobalt_hashmap_get(map, keys[i]);
        TEST_ASSERT(got != NULL);
        TEST_ASSERT(*got == values[i]);
    }

    /* Delete half and verify */
    for (int i = 0; i < 256; i += 2) {
        int ret = cobalt_hashmap_remove(map, keys[i]);
        TEST_ASSERT(ret == 0);
    }
    TEST_ASSERT(cobalt_hashmap_size(map) == 128);

    for (int i = 0; i < 256; i++) {
        int *got = (int *)cobalt_hashmap_get(map, keys[i]);
        if (i % 2 == 0) {
            TEST_ASSERT(got == NULL);
        } else {
            TEST_ASSERT(got != NULL);
            TEST_ASSERT(*got == values[i]);
        }
    }

    cobalt_hashmap_destroy(map);
    printf("  Collision stress test passed\n");
}

