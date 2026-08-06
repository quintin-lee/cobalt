/**
 * @file test_map.c
 * @brief Unit tests for the Map abstract interface
 * @details Tests polymorphic map operations through both HashMap and TreeMap.
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/container/treemap.h"
#include "cobalt/interface/map.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Helper: run map tests against a given factory                              */
/* -------------------------------------------------------------------------- */

static void run_map_tests(cobalt_map_t *map, const char *name)
{
    printf("  Testing map interface on %s...\n", name);

    /* put / get */
    static int val_a = 1;
    static int val_b = 2;
    static int val_c = 3;
    TEST_ASSERT(map->put(map, "apple", strlen("apple"), &val_a) == 0);
    TEST_ASSERT(map->put(map, "banana", strlen("banana"), &val_b) == 0);
    TEST_ASSERT(map->put(map, "cherry", strlen("cherry"), &val_c) == 0);
    TEST_ASSERT(map->size(map) == 3);
    TEST_ASSERT(!map->is_empty(map));

    int *got = (int *)map->get(map, "apple", strlen("apple"));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 1);
    got = (int *)map->get(map, "banana", strlen("banana"));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 2);
    got = (int *)map->get(map, "cherry", strlen("cherry"));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 3);

    /* update existing key */
    static int val_a2 = 99;
    TEST_ASSERT(map->put(map, "apple", strlen("apple"), &val_a2) == 0);
    got = (int *)map->get(map, "apple", strlen("apple"));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 99);

    /* remove */
    TEST_ASSERT(map->remove(map, "banana", strlen("banana")) == 0);
    TEST_ASSERT(map->size(map) == 2);
    TEST_ASSERT(map->get(map, "banana", strlen("banana")) == NULL);

    /* iterator */
    cobalt_map_iterator_t *iter = map->iterator(map);
    TEST_ASSERT(iter != NULL);
    TEST_ASSERT(map->iterator(map) != NULL); /* sanity: can create multiple */

    int count = 0;
    while (cobalt_map_iterator_has_next(iter)) {
        cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
        TEST_ASSERT(pair.key != NULL);
        TEST_ASSERT(pair.value != NULL);
        count++;
    }
    TEST_ASSERT(count == 2);
    cobalt_map_iterator_destroy(iter);

    /* remaining keys via iterator */
    iter = map->iterator(map);
    TEST_ASSERT(iter != NULL);
    cobalt_map_pair_t p1 = cobalt_map_iterator_next(iter);
    cobalt_map_pair_t p2 = cobalt_map_iterator_next(iter);
    cobalt_map_pair_t p3 = cobalt_map_iterator_next(iter);
    TEST_ASSERT(p1.key != NULL);
    TEST_ASSERT(p2.key != NULL);
    TEST_ASSERT(p3.key == NULL); /* exhausted */
    cobalt_map_iterator_destroy(iter);

    /* empty map */
    map->remove(map, "apple", strlen("apple"));
    map->remove(map, "cherry", strlen("cherry"));
    TEST_ASSERT(map->size(map) == 0);
    TEST_ASSERT(map->is_empty(map));

    map->destroy(map);
    printf("    %s map interface: PASS\n", name);
}

/* -------------------------------------------------------------------------- */
/* Generic key tests (int keys via HashMap)                                   */
/* -------------------------------------------------------------------------- */

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

static void run_map_generic_key_tests(cobalt_map_t *map, const char *name)
{
    printf("  Testing map interface with generic int keys on %s...\n", name);

    int        k1 = 10, k2 = 20, k3 = 30;
    static int v_a = 100, v_b = 200, v_c = 300;

    TEST_ASSERT(map->put(map, &k1, sizeof(int), &v_a) == 0);
    TEST_ASSERT(map->put(map, &k2, sizeof(int), &v_b) == 0);
    TEST_ASSERT(map->put(map, &k3, sizeof(int), &v_c) == 0);
    TEST_ASSERT(map->size(map) == 3);

    int *got = (int *)map->get(map, &k2, sizeof(int));
    TEST_ASSERT(got != NULL);
    TEST_EQUAL(*got, 200);

    TEST_ASSERT(map->remove(map, &k1, sizeof(int)) == 0);
    TEST_ASSERT(map->size(map) == 2);
    TEST_ASSERT(map->get(map, &k1, sizeof(int)) == NULL);

    /* iterate — collect all keys */
    cobalt_map_iterator_t *iter = map->iterator(map);
    TEST_ASSERT(iter != NULL);
    int found_20 = 0, found_30 = 0;
    while (cobalt_map_iterator_has_next(iter)) {
        cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
        int               k    = *(const int *)pair.key;
        if (k == 20) {
            found_20 = 1;
        }
        if (k == 30) {
            found_30 = 1;
        }
    }
    TEST_ASSERT(found_20 && found_30);
    cobalt_map_iterator_destroy(iter);

    map->destroy(map);
    printf("    %s generic int key: PASS\n", name);
}

/* -------------------------------------------------------------------------- */
/* Entry points                                                               */
/* -------------------------------------------------------------------------- */

void test_map_interface(void)
{
    printf("Testing map interface...\n");

    /* HashMap polymorphic tests */
    cobalt_hashmap_t *hm = cobalt_hashmap_create(8);
    TEST_ASSERT(hm != NULL);
    run_map_tests((cobalt_map_t *)hm, "hashmap");

    /* TreeMap polymorphic tests */
    cobalt_treemap_t *tm = cobalt_treemap_create();
    TEST_ASSERT(tm != NULL);
    run_map_tests((cobalt_map_t *)tm, "treemap");

    /* Generic key tests — only HashMap supports ext API */
    cobalt_hashmap_t *hm_ext = cobalt_hashmap_create_ext(8, hash_int, equal_int);
    TEST_ASSERT(hm_ext != NULL);
    run_map_generic_key_tests((cobalt_map_t *)hm_ext, "hashmap_ext");

    printf("  Map interface tests completed\n");
}
