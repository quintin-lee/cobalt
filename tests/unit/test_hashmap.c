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

void test_hashmap(void)
{
    printf("Testing hashmap...\n");
    test_hashmap_basic();
    test_hashmap_resize();
    test_hashmap_zero_initial_capacity();
    test_hashmap_resize_stress();
    printf("  Hashmap tests completed\n");
}

/* -------------------------------------------------------------------------- */
/* Generic (ext) API tests                                                    */
/* -------------------------------------------------------------------------- */

/* Djb2 hash for int keys */
static unsigned int hash_int(const void *key, size_t key_len)
{
    (void)key_len;
    const int *k = (const int *)key;
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

    int key1 = 42, key2 = 99;
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

void test_set_ext_int_elements(void)
{
    printf("Testing set ext with int elements...\n");
    cobalt_set_t *set = cobalt_set_create_ext(8, hash_int, equal_int);
    TEST_ASSERT(set != NULL);

    int a = 10, b = 20, c = 10;
    TEST_ASSERT(cobalt_set_insert_ext(set, &a, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_set_insert_ext(set, &b, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_set_insert_ext(set, &c, sizeof(int)) == 0); /* duplicate */
    TEST_ASSERT(cobalt_set_size(set) == 2);
    TEST_ASSERT(cobalt_set_contains_ext(set, &a, sizeof(int)) == 1);
    TEST_ASSERT(cobalt_set_contains_ext(set, &b, sizeof(int)) == 1);
    TEST_ASSERT(cobalt_set_contains_ext(set, &c, sizeof(int)) == 1);

    TEST_ASSERT(cobalt_set_remove_ext(set, &a, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);
    TEST_ASSERT(cobalt_set_contains_ext(set, &a, sizeof(int)) == 0);

    cobalt_set_destroy(set);
    printf("  set ext int elements: OK\n");
}
