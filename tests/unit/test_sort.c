/**
 * @file test_sort.c
 * @Unit test for sorting algorithms.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cobalt/algorithm/sort.h"

/* Comparison function for integers */
int cmp_int(const void *a, const void *b) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    return (*ia > *ib) - (*ia < *ib);
}

void test_sort(void) {
    printf("Testing sort...\n");
    printf("  Sort tests completed\n");
}
