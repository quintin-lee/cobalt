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

/* Comparison function for doubles */
int cmp_double(const void *a, const void *b) {
    const double *da = (const double *)a;
    const double *db = (const double *)b;
    if (*da < *db) return -1;
    if (*da > *db) return 1;
    return 0;
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

void test_insertion_sort(void) {
    printf("Testing insertion sort...\n");
    
    int arr[] = {5, 2, 4, 6, 1, 3};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    cobalt_insertion_sort(arr, n, sizeof(int), cmp_int);
    
    int expected[] = {1, 2, 3, 4, 5, 6};
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (arr[i] != expected[i]) {
            ok = 0;
            break;
        }
    }
    
    if (ok) {
        printf("  Insertion sort correct: OK\n");
    } else {
        fprintf(stderr, "ERROR: Insertion sort produced wrong output\n");
    }
}

void test_sort(void) {
    printf("Testing sort...\n");
    test_qsort();
    test_insertion_sort();
    printf("  Sort tests completed\n");
}
