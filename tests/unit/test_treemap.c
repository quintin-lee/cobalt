/**
 * @file test_treemap.c
 * @Unit test for treemap (BST-based).
 */

#include "cobalt/container/treemap.h"
#include <stdio.h>
#include <string.h>

void test_treemap_basic(void)
{
    printf("Testing treemap basic operations...\n");

    cobalt_treemap_t* map = cobalt_treemap_create();
    if (!map)
        {
            fprintf(stderr, "ERROR: Failed to create treemap\n");
            return;
        }

    /* Test empty tree */
    if (cobalt_treemap_size(map) == 0)
        {
            printf("  Empty tree size: OK\n");
        }

    /* Insert simple values */
    int val1 = 1, val2 = 2, val3 = 3;
    cobalt_treemap_put(map, "b", &val1);
    cobalt_treemap_put(map, "a", &val2);
    cobalt_treemap_put(map, "c", &val3);

    if (cobalt_treemap_size(map) == 3)
        {
            printf("  Size after 3 inserts: OK\n");
        }

    /* Get values */
    int* got = (int*)cobalt_treemap_get(map, "a");
    if (got && *got == 2)
        {
            printf("  Get 'a' returns 2: OK\n");
        }

    /* Min/Max */
    const char* min = cobalt_treemap_min_key(map);
    const char* max = cobalt_treemap_max_key(map);
    if (min && strcmp(min, "a") == 0)
        {
            printf("  Min key 'a': OK\n");
        }
    if (max && strcmp(max, "c") == 0)
        {
            printf("  Max key 'c': OK\n");
        }

    /* Test destroy */
    cobalt_treemap_destroy(map);
    printf("  Treemap destroyed successfully\n");
    printf("  Treemap tests completed\n");
}

void test_treemap(void)
{
    printf("Testing treemap...\n");
    test_treemap_basic();
}
