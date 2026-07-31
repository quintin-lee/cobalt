/**
 * @file test_hashmap.c
 * @Unit test for hash map container.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cobalt/container/hashmap.h"

void test_hashmap_basic(void) {
    printf("Testing hashmap basic operations...\n");
    
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    if (!map) {
        fprintf(stderr, "ERROR: Failed to create hashmap\n");
        return;
    }
    
    /* Simple values - don't worry about freeing them, hashmap will free nodes */
    static int val1 = 100;
    static int val2 = 200;
    
    int ret = cobalt_hashmap_put(map, "name", &val1);
    if (ret == 0) printf("  Put 'name': OK\n");
    
    ret = cobalt_hashmap_put(map, "age", &val2);
    if (ret == 0) printf("  Put 'age': OK\n");
    
    int *got = (int *)cobalt_hashmap_get(map, "name");
    if (got && *got == 100) printf("  Get 'name' returns 100: OK\n");
    
    got = (int *)cobalt_hashmap_get(map, "age");
    if (got && *got == 200) printf("  Get 'age' returns 200: OK\n");
    
    if (cobalt_hashmap_size(map) == 2) printf("  Size is 2: OK\n");
    
    ret = cobalt_hashmap_remove(map, "name");
    if (ret == 0) printf("  Remove 'name': OK\n");
    
    if (cobalt_hashmap_size(map) == 1) printf("  Size after remove is 1: OK\n");
    
    got = (int *)cobalt_hashmap_get(map, "name");
    if (got == NULL) printf("  Removed key returns NULL: OK\n");
    
    cobalt_hashmap_destroy(map);
    printf("  Hashmap tests completed\n");
}

void test_hashmap_null_safety(void) {
    printf("Testing hashmap null safety...\n");
    if (cobalt_hashmap_get(NULL, "key") == NULL) printf("  Null get: OK\n");
    if (cobalt_hashmap_size(NULL) == 0) printf("  Null size: OK\n");
}

void test_hashmap(void) {
    printf("Testing hashmap...\n");
    test_hashmap_basic();
    test_hashmap_null_safety();
    printf("  Hashmap tests completed\n");
}
