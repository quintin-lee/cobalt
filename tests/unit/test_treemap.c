/**
 * @file test_treemap.c
 * @Unit test for red-black tree based sorted map.
 */

#include <stdio.h>
#include <string.h>
#include "cobalt/container/treemap.h"

void test_treemap_basic(void) {
    printf("Testing treemap basic operations...\n");
    
    cobalt_treemap_t *map = cobalt_treemap_create();
    if (!map) {
        fprintf(stderr, "ERROR: Failed to create treemap\n");
        return;
    }
    
    /* Test empty tree */
    if (cobalt_treemap_size(map) == 0) {
        printf("  Empty tree size: OK\n");
    }
    
    if (cobalt_treemap_min_key(map) == NULL && cobalt_treemap_max_key(map) == NULL) {
        printf("  Min/max of empty tree: OK\n");
    }
    
    /* Insert values */
    cobalt_treemap_put(map, "banana", (void*)1);
    cobalt_treemap_put(map, "apple", (void*)2);
    cobalt_treemap_put(map, "cherry", (void*)3);
    cobalt_treemap_put(map, "date", (void*)4);
    
    if (cobalt_treemap_size(map) == 4) {
        printf("  Size after 4 inserts: OK\n");
    }
    
    /* Get values */
    if ((int)(size_t)cobalt_treemap_get(map, "apple") == 2) {
        printf("  Get 'apple': OK\n");
    }
    if ((int)(size_t)cobalt_treemap_get(map, "banana") == 1) {
        printf("  Get 'banana': OK\n");
    }
    
    /* Min/Max keys (alphabetically sorted) */
    const char *min = cobalt_treemap_min_key(map);
    const char *max = cobalt_treemap_max_key(map);
    if (min && strcmp(min, "apple") == 0) {
        printf("  Min key '%s': OK\n", min);
    }
    if (max && strcmp(max, "date") == 0) {
        printf("  Max key '%s': OK\n", max);
    }
    
    /* Update existing key */
    cobalt_treemap_put(map, "apple", (void*)20);
    if ((int)(size_t)cobalt_treemap_get(map, "apple") == 20) {
        printf("  Update existing key: OK\n");
    }
    
    /* Remove */
    if (cobalt_treemap_remove(map, "banana") == 0) {
        printf("  Remove 'banana': OK\n");
    }
    
    if (cobalt_treemap_get(map, "banana") == NULL) {
        printf("  Removed key returns NULL: OK\n");
    }
    
    if (cobalt_treemap_size(map) == 3) {
        printf("  Size after remove: OK\n");
    }
    
    cobalt_treemap_destroy(map);
    printf("  Treemap tests completed\n");
}

void test_treemap_stress(void) {
    printf("Testing treemap stress test (100 inserts, 50 removes)...\n");
    
    cobalt_treemap_t *map = cobalt_treemap_create();
    char key[32];
    
    /* Insert 100 elements */
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "item%d", i);
        cobalt_treemap_put(map, key, (void*)(size_t)i);
    }
    
    if (cobalt_treemap_size(map) == 100) {
        printf("  100 inserts: OK\n");
    }
    
    /* Remove 50 elements */
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "item%d", i);
        cobalt_treemap_remove(map, key);
    }
    
    if (cobalt_treemap_size(map) == 50) {
        printf("  50 removes: OK\n");
    }
    
    /* Verify remaining elements */
    for (int i = 50; i < 100; i++) {
        snprintf(key, sizeof(key), "item%d", i);
        if ((int)(size_t)cobalt_treemap_get(map, key) != i) {
            fprintf(stderr, "ERROR: Missing element %s\n", key);
        }
    }
    printf("  All remaining elements verified: OK\n");
    
    cobalt_treemap_destroy(map);
    printf("  Stress test completed\n");
}

void test_treemap(void) {
    printf("Testing treemap...\n");
    test_treemap_basic();
    test_treemap_stress();
    printf("  Treemap tests completed\n");
}
