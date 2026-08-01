/**
 * @file sort_example.c
 * @brief Demonstrates sorting algorithms from Cobalt's algorithm layer
 *
 * Shows:
 * - Defining comparison functions
 * - Using qsort and insertion_sort
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>

/* Comparison function for integers */
int cmp_int(const void* a, const void* b)
{
    const int* ia = (const int*)a;
    const int* ib = (const int*)b;
    return (*ia > *ib) - (*ia < *ib);
}

/* Comparison function for double */
int cmp_double(const void* a, const void* b)
{
    const double* da = (const double*)a;
    const double* db = (const double*)b;
    if (*da < *db)
        return -1;
    if (*da > *db)
        return 1;
    return 0;
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Example 1: Quick sort an array of integers */
    int ints[] = {64, 34, 25, 12, 22, 11, 90};
    size_t n = sizeof(ints) / sizeof(ints[0]);

    cobalt_info("Before quicksort: ");
    for (size_t i = 0; i < n; i++)
        {
            cobalt_info("%d ", ints[i]);
        }
    cobalt_info("\n");

    cobalt_qsort(ints, n, sizeof(int), cmp_int);

    cobalt_info("After quicksort: ");
    for (size_t i = 0; i < n; i++)
        {
            cobalt_info("%d ", ints[i]);
        }
    cobalt_info("\n\n");

    /* Example 2: Insertion sort for small arrays */
    double doubles[] = {3.14, 2.71, 1.41, 0.57, 1.73};
    size_t m = sizeof(doubles) / sizeof(doubles[0]);

    cobalt_info("Before insertion sort: ");
    for (size_t i = 0; i < m; i++)
        {
            cobalt_info("%.2f ", doubles[i]);
        }
    cobalt_info("\n");

    cobalt_insertion_sort(doubles, m, sizeof(double), cmp_double);

    cobalt_info("After insertion sort: ");
    for (size_t i = 0; i < m; i++)
        {
            cobalt_info("%.2f ", doubles[i]);
        }
    cobalt_info("\n");

    cobalt_info("Sort demo complete!\n");
    return 0;
}