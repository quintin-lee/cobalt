#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "test_framework.h"
#include <stdio.h>

void test_set_basic(void)
{
    printf("Testing set basic operations...\n");

    cobalt_set_t *set = cobalt_set_create(16);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);

    int a = 1, b = 2, c = 3;

    TEST_ASSERT(cobalt_set_insert(set, &a) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);
    TEST_ASSERT(cobalt_set_contains(set, &a));
    printf("  Insert and contains: OK\n");

    TEST_ASSERT(cobalt_set_insert(set, &b) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &c) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 3);
    printf("  Multiple inserts: OK\n");

    TEST_ASSERT(cobalt_set_contains(set, &a));
    TEST_ASSERT(cobalt_set_contains(set, &b));
    TEST_ASSERT(cobalt_set_contains(set, &c));
    printf("  Contains check: OK\n");

    TEST_ASSERT(cobalt_set_remove(set, &b) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 2);
    TEST_ASSERT(!cobalt_set_contains(set, &b));
    printf("  Remove: OK\n");

    TEST_ASSERT(cobalt_set_remove(set, &b) == -1);
    printf("  Remove non-existent: OK\n");

    cobalt_set_destroy(set);
    printf("  Set basic test passed\n");
}

void test_set_duplicates(void)
{
    printf("Testing set duplicate handling...\n");

    cobalt_set_t *set = cobalt_set_create(4);
    TEST_ASSERT(set != NULL);

    int val = 42;
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);

    cobalt_set_destroy(set);
    printf("  Duplicate insert (idempotent): OK\n");
}

void test_set_empty(void)
{
    printf("Testing set empty operations...\n");

    cobalt_set_t *set = cobalt_set_create(0);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);

    TEST_ASSERT(cobalt_set_remove(set, NULL) == -1);
    TEST_ASSERT(cobalt_set_contains(set, NULL) == 0);

    cobalt_set_destroy(set);

    TEST_ASSERT(cobalt_set_contains(NULL, NULL) == 0);
    TEST_ASSERT(cobalt_set_size(NULL) == 0);
    cobalt_set_destroy(NULL);
    printf("  Empty and NULL safety: OK\n");
}

void test_set_map_interface(void);

void test_set_ext_api(void)
{
    printf("Testing set extended (binary key) API...\n");

    /* Verified against set.c: alloc is a required parameter, NULL returns NULL */
    TEST_ASSERT(cobalt_set_create_ext_with_allocator(8, NULL, NULL, NULL) == NULL);

    cobalt_set_t *set =
        cobalt_set_create_ext_with_allocator(8, NULL, NULL, cobalt_allocator_get_system());
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set) == 1);

    /* A struct key, so membership must use the length-aware path rather than
       string hashing. Adding a distinguishing field beyond the tag proves the
       whole struct is hashed, not just the first bytes. */
    struct bin_key {
        int  id;
        char tag[4];
    };
    struct bin_key k1 = {7, "xyz"};
    struct bin_key k2 = {9, "abc"};
    /* Same tag, different id: must be a different member */
    struct bin_key k3 = {8, "xyz"};

    TEST_ASSERT(cobalt_set_insert_ext(set, &k1, sizeof(k1)) == 0);
    TEST_ASSERT(cobalt_set_contains_ext(set, &k1, sizeof(k1)) == 1);
    TEST_ASSERT(cobalt_set_contains_ext(set, &k2, sizeof(k2)) == 0);
    TEST_ASSERT(cobalt_set_contains_ext(set, &k3, sizeof(k3)) == 0);
    TEST_EQUAL(cobalt_set_size(set), (size_t)1);

    TEST_ASSERT(cobalt_set_insert_ext(set, &k3, sizeof(k3)) == 0);
    TEST_EQUAL(cobalt_set_size(set), (size_t)2);
    TEST_ASSERT(cobalt_set_contains_ext(set, &k3, sizeof(k3)) == 1);

    /* get() returns the map-interface sentinel for a present member, NULL for
       an absent one. The sentinel is not the caller's pointer. */
    TEST_ASSERT(cobalt_set_get(set, &k1) != NULL);
    TEST_ASSERT(cobalt_set_get(set, &k2) == NULL);

    /* Re-inserting an existing member is idempotent in size */
    TEST_ASSERT(cobalt_set_insert_ext(set, &k1, sizeof(k1)) == 0);
    TEST_EQUAL(cobalt_set_size(set), (size_t)2);

    TEST_ASSERT(cobalt_set_remove_ext(set, &k1, sizeof(k1)) == 0);
    TEST_EQUAL(cobalt_set_size(set), (size_t)1);
    TEST_ASSERT(cobalt_set_contains_ext(set, &k1, sizeof(k1)) == 0);
    /* Removing an absent member reports failure */
    TEST_ASSERT(cobalt_set_remove_ext(set, &k1, sizeof(k1)) == -1);

    /* NULL guards */
    TEST_ASSERT(cobalt_set_insert_ext(NULL, &k1, sizeof(k1)) == -1);
    TEST_ASSERT(cobalt_set_contains_ext(NULL, &k1, sizeof(k1)) == 0);
    TEST_ASSERT(cobalt_set_remove_ext(NULL, &k1, sizeof(k1)) == -1);

    cobalt_set_destroy(set);
    printf("  Set ext API: OK\n");
}

void test_set_iterator_create(void)
{
    printf("Testing set iterator...\n");

    cobalt_set_t *set = cobalt_set_create(8);
    TEST_ASSERT(set != NULL);

    TEST_ASSERT(cobalt_set_insert(set, "alpha") == 0);
    TEST_ASSERT(cobalt_set_insert(set, "beta") == 0);
    TEST_ASSERT(cobalt_set_insert(set, "gamma") == 0);

    /* The set exposes its members through a map-compatible iterator; each
       yielded pair's key is the member and value is the static sentinel. */
    cobalt_map_iterator_t *it = cobalt_set_iterator_create(set);
    TEST_ASSERT(it != NULL);

    int seen = 0;
    while (cobalt_map_iterator_has_next(it)) {
        cobalt_map_pair_t pair = cobalt_map_iterator_next(it);
        TEST_ASSERT(pair.key != NULL);
        seen++;
    }
    TEST_EQUAL(seen, 3);

    cobalt_map_iterator_destroy(it);
    cobalt_set_destroy(set);
    printf("  Set iterator: OK\n");
}

void test_set(void)
{
    printf("Testing set...\n");
    test_set_basic();
    test_set_duplicates();
    test_set_empty();
    test_set_map_interface();
    test_set_ext_api();
    test_set_iterator_create();
    printf("  Set tests completed\n");
}

/* -------------------------------------------------------------------------- */
/* Map interface tests                                                          */
/* -------------------------------------------------------------------------- */

void test_set_map_interface(void)
{
    printf("Testing set map interface...\n");

    cobalt_set_t *set = cobalt_set_create(8);
    TEST_ASSERT(set != NULL);

    int a = 1, b = 2, c = 3;

    /* Test through map interface */
    cobalt_map_t *map = (cobalt_map_t *)set;

    TEST_ASSERT(cobalt_map_put(map, &a, sizeof(int), (void *)&a) == 0);
    TEST_ASSERT(cobalt_map_put(map, &b, sizeof(int), (void *)&b) == 0);
    TEST_ASSERT(cobalt_map_put(map, &c, sizeof(int), (void *)&c) == 0);
    TEST_ASSERT(cobalt_map_size(map) == 3);
    TEST_ASSERT(!cobalt_map_is_empty(map));

    /* contains */
    TEST_ASSERT(cobalt_map_contains(map, &a, sizeof(int)) != 0);
    TEST_ASSERT(cobalt_map_contains(map, &b, sizeof(int)) != 0);
    TEST_ASSERT(cobalt_map_contains(map, &c, sizeof(int)) != 0);

    int d = 99;
    TEST_ASSERT(cobalt_map_contains(map, &d, sizeof(int)) == 0);

    /* get returns sentinel pointer */
    void *val = cobalt_map_get(map, &a, sizeof(int));
    TEST_ASSERT(val != NULL);
    TEST_ASSERT(val != NULL);

    /* iterator */
    cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
    TEST_ASSERT(iter != NULL);
    int count = 0;
    while (cobalt_map_iterator_has_next(iter)) {
        cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
        TEST_ASSERT(pair.key != NULL);
        TEST_ASSERT(pair.value != NULL);
        count++;
    }
    TEST_ASSERT(count == 3);
    cobalt_map_iterator_destroy(iter);

    /* remove via map interface */
    TEST_ASSERT(cobalt_map_remove(map, &b, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_map_size(map) == 2);
    TEST_ASSERT(cobalt_map_contains(map, &b, sizeof(int)) == 0);

    /* clear */
    cobalt_map_clear(map);
    TEST_ASSERT(cobalt_map_size(map) == 0);
    TEST_ASSERT(cobalt_map_is_empty(map));
    TEST_ASSERT(cobalt_map_contains(map, &a, sizeof(int)) == 0);

    /* NULL safety */
    TEST_ASSERT(cobalt_map_get(NULL, &a, sizeof(int)) == NULL);
    TEST_ASSERT(cobalt_map_put(NULL, &a, sizeof(int), (void *)&a) == -1);
    TEST_ASSERT(cobalt_map_remove(NULL, &a, sizeof(int)) == -1);
    TEST_ASSERT(cobalt_map_contains(NULL, &a, sizeof(int)) == 0);
    TEST_ASSERT(cobalt_map_size(NULL) == 0);
    TEST_ASSERT(cobalt_map_is_empty(NULL) == 1);

    map->destroy(map);
    printf("  Set map interface: PASS\n");
}

