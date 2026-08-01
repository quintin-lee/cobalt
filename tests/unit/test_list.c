/**
 * @file test_list.c
 * @brief Unit test for doubly-linked list container.
 */

#include "cobalt/container/list.h"
#include "cobalt/interface/iterator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_list_basic(void)
{
    printf("Testing list basic operations...\n");

    cobalt_list_t* list = cobalt_list_create();
    if (!list)
        {
            fprintf(stderr, "ERROR: Failed to create list\n");
            return;
        }

    /* Should be empty initially */
    if (cobalt_list_is_empty(list))
        {
            printf("  List is empty: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: New list should be empty\n");
        }

    if (cobalt_list_size(list) == 0)
        {
            printf("  Size is 0: OK\n");
        }

    /* Push front */
    int val1 = 10;
    int ret = cobalt_list_push_front(list, &val1);
    if (ret == 0)
        {
            printf("  Push front: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Push front failed\n");
        }

    /* Push back */
    int val2 = 20;
    ret = cobalt_list_push_back(list, &val2);
    if (ret == 0)
        {
            printf("  Push back: OK\n");
        }

    /* Pop front */
    void* item = cobalt_list_pop_front(list);
    if (item && *((int*)item) == 10)
        {
            printf("  Pop front returns 10: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Expected 10 from pop front\n");
        }

    if (cobalt_list_size(list) == 1)
        {
            printf("  Size after pop front: OK\n");
        }

    /* Pop back */
    item = cobalt_list_pop_back(list);
    if (item && *((int*)item) == 20)
        {
            printf("  Pop back returns 20: OK\n");
        }

    if (cobalt_list_size(list) == 0)
        {
            printf("  Size after pop back: OK\n");
        }

    /* Should be empty again */
    if (cobalt_list_is_empty(list))
        {
            printf("  List is empty after pops: OK\n");
        }

    cobalt_list_destroy(list);
}

void test_list_edge_cases(void)
{
    printf("Testing list edge cases...\n");

    cobalt_list_t* list = cobalt_list_create();

    /* Pop from empty list should return NULL */
    void* item = cobalt_list_pop_front(list);
    if (item == NULL)
        {
            printf("  Pop empty list returns NULL: OK\n");
        }

    item = cobalt_list_pop_back(list);
    if (item == NULL)
        {
            printf("  Pop back empty list returns NULL: OK\n");
        }

    /* Get from empty list */
    item = cobalt_list_get(list, 0);
    if (item == NULL)
        {
            printf("  Get from empty list returns NULL: OK\n");
        }

    /* Push to NULL list should fail safely */
    int val = 42;
    int ret = cobalt_list_push_front(NULL, &val);
    if (ret == -1)
        {
            printf("  Push to NULL list fails gracefully: OK\n");
        }

    cobalt_list_destroy(list);

    /* Test NULL destroy */
    cobalt_list_destroy(NULL);
    printf("  Destroy NULL list: OK\n");
}

void test_list(void)
{
    printf("Testing list...\n");
    test_list_basic();
    test_list_edge_cases();
    printf("  List tests completed\n");
}
