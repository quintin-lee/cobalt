/**
 * @file test_list.c
 * @Unit test for doubly-linked list container.
 */

#include <stdio.h>
#include <assert.h>
#include "cobalt/container/list.h"

void test_list_basic(void) {
    printf("Testing list basic operations...\n");
    
    cobalt_list_t *list = cobalt_list_create();
    if (!list) {
        fprintf(stderr, "ERROR: Failed to create list\n");
        return;
    }
    
    /* Should be empty initially */
    if (cobalt_list_is_empty(list)) {
        printf("  List is empty: OK\n");
    } else {
        fprintf(stderr, "ERROR: New list should be empty\n");
    }
    
    if (cobalt_list_size(list) == 0) {
        printf("  Size is 0: OK\n");
    }
    
    /* Push front */
    int val1 = 10;
    cobalt_list_push_front(list, &val1);
    if (cobalt_list_size(list) == 1) {
        printf("  Push front size=1: OK\n");
    }
    
    /* Push back */
    int val2 = 20;
    cobalt_list_push_back(list, &val2);
    if (cobalt_list_size(list) == 2) {
        printf("  Push back size=2: OK\n");
    }
    
    /* Pop front */
    void *item = cobalt_list_pop_front(list);
    if (item && *((int*)item) == 10) {
        printf("  Pop front returns 10: OK\n");
    } else {
        fprintf(stderr, "ERROR: Expected 10 from pop front\n");
    }
    
    if (cobalt_list_size(list) == 1) {
        printf("  Size after pop=1: OK\n");
    }
    
    /* Clean up */
    cobalt_list_destroy(list);
    printf("  List tests completed\n");
}

void test_list_edge_cases(void) {
    printf("Testing list edge cases...\n");
    
    cobalt_list_t *list = cobalt_list_create();
    
    /* Pop from empty list should return NULL */
    void *item = cobalt_list_pop_front(list);
    if (item == NULL) {
        printf("  Pop empty list returns NULL: OK\n");
    }
    
    /* Push to NULL list should fail safely */
    int ret = cobalt_list_push_front(NULL, &item);
    if (ret == -1) {
        printf("  Push to NULL list fails gracefully: OK\n");
    }
    
    cobalt_list_destroy(list);
}

void test_list(void) {
    printf("Testing list...\n");
    test_list_basic();
    test_list_edge_cases();
    printf("  List tests completed\n");
}
