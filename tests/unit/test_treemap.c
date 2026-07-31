/**
 * @file test_treemap.c
 * @Unit test for treemap.
 */

#include <stdio.h>
#include "cobalt/container/treemap.h"

void test_treemap_basic(void) {
    printf("Testing treemap basic operations...\n");
    
    cobalt_treemap_t *map = cobalt_treemap_create();
    if (!map) {
        fprintf(stderr, "ERROR: Failed to create treemap\n");
        return;
    }
    printf("  Treemap created\n");
    
    /* Test size is zero initially */
    if (cobalt_treemap_size(map) == 0) {
        printf("  Initial size is 0: OK\n");
    }
    
    /* Clean up */
    cobalt_treemap_destroy(map);
    printf("  Treemap destroyed\n");
}

void test_treemap(void) {
    printf("Testing treemap...\n");
    test_treemap_basic();
    printf("  Treemap tests completed\n");
}
