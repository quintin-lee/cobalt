/**
 * @file test_treemap.c
 * @brief Unit test for treemap (BST-based).
 */

#include "cobalt/container/treemap.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

void test_treemap_iterator(void);
void test_treemap_edge_cases(void);
void test_treemap_duplicate_keys(void);
void test_treemap_large_scale(void);
void test_treemap_basic(void)
{
    printf("Testing treemap basic operations...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    TEST_ASSERT(cobalt_treemap_size(map) == 0);
    printf("  Empty tree size: OK\n");

    int val1 = 1;
    int val2 = 2;
    int val3 = 3;
    TEST_ASSERT(cobalt_treemap_put(map, "b", &val1) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "a", &val2) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "c", &val3) == 0);

    TEST_ASSERT(cobalt_treemap_size(map) == 3);
    printf("  Size after 3 inserts: OK\n");

    int *got = (int *)cobalt_treemap_get(map, "a");
    TEST_ASSERT(got != NULL);
    TEST_ASSERT(*got == 2);
    printf("  Get 'a' returns 2: OK\n");

    const char *min = cobalt_treemap_min_key(map);
    const char *max = cobalt_treemap_max_key(map);
    TEST_ASSERT(min != NULL && strcmp(min, "a") == 0);
    TEST_ASSERT(max != NULL && strcmp(max, "c") == 0);
    printf("  Min key 'a': OK\n");
    printf("  Max key 'c': OK\n");

    cobalt_treemap_destroy(map);
    printf("  Treemap destroyed successfully\n");
    printf("  Treemap tests completed\n");
}

void test_treemap_remove(void)
{
    printf("Testing treemap remove...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    int a = 1;
    int b = 2;
    int c = 3;
    TEST_ASSERT(cobalt_treemap_put(map, "a", &a) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "b", &b) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "c", &c) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 3);

    TEST_ASSERT(cobalt_treemap_remove(map, "b") == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 2);
    TEST_ASSERT(cobalt_treemap_get(map, "b") == NULL);
    TEST_ASSERT(cobalt_treemap_get(map, "a") == &a);
    TEST_ASSERT(cobalt_treemap_get(map, "c") == &c);
    printf("  Remove middle element: OK\n");

    TEST_ASSERT(cobalt_treemap_remove(map, "a") == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(cobalt_treemap_get(map, "a") == NULL);
    printf("  Remove root element: OK\n");

    TEST_ASSERT(cobalt_treemap_remove(map, "c") == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 0);
    TEST_ASSERT(cobalt_treemap_get(map, "c") == NULL);
    printf("  Remove last element: OK\n");

    TEST_ASSERT(cobalt_treemap_remove(map, "a") == -1);
    printf("  Remove from empty tree: OK\n");

    int d = 4;
    TEST_ASSERT(cobalt_treemap_put(map, "a", &d) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(cobalt_treemap_get(map, "a") == &d);
    printf("  Re-insert after removal: OK\n");

    cobalt_treemap_destroy(map);
    printf("  TreeMap remove test passed\n");
}

void test_treemap_remove_stress(void)
{
    printf("Testing treemap remove stress (memory leak check)...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        int *val = malloc(sizeof(int));
        TEST_ASSERT(val != NULL);
        *val = i;
        TEST_ASSERT(cobalt_treemap_put(map, key, val) == 0);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 100);

    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        void *val = cobalt_treemap_get(map, key);
        int   ret = cobalt_treemap_remove(map, key);
        if (ret == 0) {
            free(val);
        }
        if (ret != 0) {
            printf("  Remove failed for %s at iteration %d, size=%zu\n",
                   key,
                   i,
                   cobalt_treemap_size(map));
        }
    }
    printf("  Size after removing 50: %zu (expected 50)\n", cobalt_treemap_size(map));
    TEST_ASSERT(cobalt_treemap_size(map) == 50);

    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) == NULL);
    }
    for (int i = 50; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) != NULL);
    }

    for (int i = 50; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        void *val = cobalt_treemap_get(map, key);
        TEST_ASSERT(cobalt_treemap_remove(map, key) == 0);
        free(val);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 0);

    cobalt_treemap_destroy(map);
    printf("  TreeMap remove stress test passed\n");
}

void test_treemap(void)
{
    printf("Testing treemap...\n");
    test_treemap_basic();
    test_treemap_remove();
    test_treemap_remove_stress();
    test_treemap_iterator();
    test_treemap_edge_cases();
    test_treemap_duplicate_keys();
    test_treemap_large_scale();
}

void test_treemap_iterator(void)
{
    printf("Testing treemap iterator...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    /* Empty tree iterator */
    cobalt_map_iterator_t *it = cobalt_treemap_iterator_create(map);
    TEST_ASSERT(it != NULL);
    cobalt_map_pair_t pair = cobalt_map_iterator_next(it);
    TEST_ASSERT(pair.key == NULL);
    cobalt_map_iterator_destroy(it);
    printf("  Empty tree iterator: OK\n");

    /* Insert and iterate */
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    TEST_ASSERT(cobalt_treemap_put(map, "b", &v1) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "a", &v2) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "c", &v3) == 0);

    it = cobalt_treemap_iterator_create(map);
    TEST_ASSERT(it != NULL);
    int         count    = 0;
    const char *prev_key = NULL;
    while ((pair = cobalt_map_iterator_next(it)).key != NULL) {
        const char *k = (const char *)pair.key;
        if (prev_key != NULL) {
            TEST_ASSERT(strcmp(k, prev_key) > 0);
        }
        prev_key = k;
        count++;
    }
    TEST_ASSERT(count == 3);
    cobalt_map_iterator_destroy(it);
    printf("  Iterator order (a, b, c): OK\n");

    /* Iterate after removal */
    cobalt_treemap_remove(map, "b");
    it       = cobalt_treemap_iterator_create(map);
    count    = 0;
    prev_key = NULL;
    while ((pair = cobalt_map_iterator_next(it)).key != NULL) {
        const char *k = (const char *)pair.key;
        if (prev_key != NULL) {
            TEST_ASSERT(strcmp(k, prev_key) > 0);
        }
        prev_key = k;
        count++;
    }
    TEST_ASSERT(count == 2);
    cobalt_map_iterator_destroy(it);
    printf("  Iterator after removal: OK\n");

    cobalt_treemap_destroy(map);
}

void test_treemap_edge_cases(void)
{
    printf("Testing treemap edge cases...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    /* Single element */
    int val = 42;
    TEST_ASSERT(cobalt_treemap_put(map, "only", &val) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(cobalt_treemap_get(map, "only") == &val);
    printf("  Single element: OK\n");

    /* Min/max on single element */
    TEST_ASSERT(cobalt_treemap_min_key(map) != NULL);
    TEST_ASSERT(cobalt_treemap_max_key(map) != NULL);
    TEST_ASSERT(strcmp(cobalt_treemap_min_key(map), "only") == 0);
    TEST_ASSERT(strcmp(cobalt_treemap_max_key(map), "only") == 0);
    printf("  Min/max on single element: OK\n");

    /* Remove the only element */
    TEST_ASSERT(cobalt_treemap_remove(map, "only") == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 0);
    TEST_ASSERT(cobalt_treemap_get(map, "only") == NULL);
    printf("  Remove single element: OK\n");

    /* Min/max on empty tree */
    TEST_ASSERT(cobalt_treemap_min_key(map) == NULL);
    TEST_ASSERT(cobalt_treemap_max_key(map) == NULL);
    printf("  Min/max on empty tree: OK\n");

    /* Remove from empty tree */
    TEST_ASSERT(cobalt_treemap_remove(map, "nope") == -1);
    printf("  Remove from empty tree: OK\n");

    /* Get from empty tree */
    TEST_ASSERT(cobalt_treemap_get(map, "nope") == NULL);
    printf("  Get from empty tree: OK\n");

    cobalt_treemap_destroy(map);
}

void test_treemap_duplicate_keys(void)
{
    printf("Testing treemap duplicate key handling...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    TEST_ASSERT(cobalt_treemap_put(map, "key", &v1) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(*((int *)cobalt_treemap_get(map, "key")) == 1);

    TEST_ASSERT(cobalt_treemap_put(map, "key", &v2) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(*((int *)cobalt_treemap_get(map, "key")) == 2);

    TEST_ASSERT(cobalt_treemap_put(map, "key", &v3) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 1);
    TEST_ASSERT(*((int *)cobalt_treemap_get(map, "key")) == 3);

    printf("  Duplicate key overwrite: OK\n");

    cobalt_treemap_destroy(map);
}

void test_treemap_large_scale(void)
{
    printf("Testing treemap large-scale insert/remove...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    /* Insert 500 elements */
    for (int i = 0; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        int *val = malloc(sizeof(int));
        TEST_ASSERT(val != NULL);
        *val = i;
        TEST_ASSERT(cobalt_treemap_put(map, key, val) == 0);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 500);
    printf("  Inserted 500 elements: OK\n");

    /* Verify min/max */
    const char *min_k = cobalt_treemap_min_key(map);
    const char *max_k = cobalt_treemap_max_key(map);
    TEST_ASSERT(min_k != NULL);
    TEST_ASSERT(max_k != NULL);
    TEST_ASSERT(strcmp(min_k, "item_0000") == 0);
    TEST_ASSERT(strcmp(max_k, "item_0499") == 0);
    printf("  Min/max after 500 inserts: OK\n");

    /* Remove half */
    for (int i = 0; i < 250; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        void *val = cobalt_treemap_get(map, key);
        TEST_ASSERT(val != NULL);
        TEST_ASSERT(cobalt_treemap_remove(map, key) == 0);
        free(val);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 250);
    printf("  Removed 250 elements: OK\n");

    /* Verify remaining */
    for (int i = 0; i < 250; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) == NULL);
    }
    for (int i = 250; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) != NULL);
    }
    printf("  Remaining elements verified: OK\n");

    /* Clean up */
    for (int i = 250; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof(key), "item_%04d", i);
        free(cobalt_treemap_get(map, key));
    }
    cobalt_treemap_destroy(map);
}
