/**
 * @file test_hashmap.c
 * @Unit test for hash map container.
 */

#include "cobalt/container/hashmap.h"
#include <stdio.h>
#include <stdlib.h>

void test_hashmap_basic(void)
{
    printf("Testing hashmap basic operations...\n");

    cobalt_hashmap_t* map = cobalt_hashmap_create(16);
    if (!map)
        {
            fprintf(stderr, "ERROR: Failed to create hashmap\n");
            return;
        }
    printf("  Created hashmap\n");

    /* Use static values to avoid heap issues */
    static int val1 = 100;
    static int val2 = 200;

    int ret = cobalt_hashmap_put(map, "name", &val1);
    printf("  Put 'name': %s\n", ret == 0 ? "OK" : "FAIL");

    ret = cobalt_hashmap_put(map, "age", &val2);
    printf("  Put 'age': %s\n", ret == 0 ? "OK" : "FAIL");

    int* got = (int*)cobalt_hashmap_get(map, "name");
    if (got && *got == 100)
        {
            printf("  Get 'name' returns 100: OK\n");
        }
    else
        {
            printf("  Get 'name': FAIL\n");
        }

    size_t sz = cobalt_hashmap_size(map);
    printf("  Size is %zu\n", sz);

    cobalt_hashmap_destroy(map);
    printf("  Hashmap tests completed\n");
}

void test_hashmap(void)
{
    printf("Testing hashmap...\n");
    test_hashmap_basic();
}
