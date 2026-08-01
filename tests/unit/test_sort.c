/**
 * @file test_sort.c
 * @Unit test for sorting algorithms.
 */

#include <stdio.h>
#include "cobalt/algorithm/sort.h"

/* Comparison function for integers */
int cmp_int(const void *a, const void *b) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    return (*ia > *ib) - (*ia < *ib);
}

void test_qsort(void) {
    printf("Testing quick sort...\n");
    
    int arr[] = {64, 34, 25, 12, 22, 11, 90};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    cobalt_qsort(arr, n, sizeof(int), cmp_int);
    
    int expected[] = {11, 12, 22, 25, 34, 64, 90};
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (arr[i] != expected[i]) {
            ok = 0;
            break;
        }
    }
    
    if (ok) {
        printf("  Quick sort correct: OK\n");
    } else {
        fprintf(stderr, "ERROR: Quick sort produced wrong output\n");
    }
}

void test_sort(void) {
    printf("Testing sort...\n");
    test_qsort();
    printf("  Sort tests completed\n");
}
