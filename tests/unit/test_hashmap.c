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
    
    /* Use heap-allocated values for proper lifecycle management */
    int *val1 = malloc(sizeof(int));
    int *val2 = malloc(sizeof(int));
    if (!val1 || !val2) {
        free(val1);
        free(val2);
        cobalt_hashmap_destroy(map);
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        return;
    }
    *val1 = 100;
    *val2 = 200;
    
    /* Insert */
    int ret = cobalt_hashmap_put(map, "name", val1);
    if (ret == 0) printf("  Put 'name': OK\n");
    else fprintf(stderr, "ERROR: Put failed\n");
    
    ret = cobalt_hashmap_put(map, "age", val2);
    if (ret == 0) printf("  Put 'age': OK\n");
    
    /* Get */
    int *got = (int *)cobalt_hashmap_get(map, "name");
    if (got && *got == 100) printf("  Get 'name' returns 100: OK\n");
    else fprintf(stderr, "ERROR: Get failed\n");
    
    got = (int *)cobalt_hashmap_get(map, "age");
    if (got && *got == 200) printf("  Get 'age' returns 200: OK\n");
    
    /* Size */
    if (cobalt_hashmap_size(map) == 2) printf("  Size is 2: OK\n");
    else fprintf(stderr, "ERROR: Expected size 2\n");
    
    /* Free user values BEFORE destroy (hashmap doesn't free values) */
    free(val1);
    free(val2);
    
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
