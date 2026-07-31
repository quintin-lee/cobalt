/**
 * @file test_hashmap.c
 * @Unit test for hash map container.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cobalt/container/hashmap.h"

void test_hashmap_basic(void) {
    printf("Testing hashmap...\n");
    
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    if (!map) {
        fprintf(stderr, "ERROR: Failed to create\n");
        return;
    }
    
    int val = 42;
    cobalt_hashmap_put(map, "key", &val);
    int *got = (int *)cobalt_hashmap_get(map, "key");
    
    if (got && *got == 42) {
        printf("  Basic get/put: OK\n");
    } else {
        printf("  Basic get/put: FAILED\n");
    }
    
    cobalt_hashmap_destroy(map);
    printf("  Hashmap tests completed\n");
}

void test_hashmap(void) {
    test_hashmap_basic();
}
