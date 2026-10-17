/**
 * @file benchmark_sort.c
 * @brief Benchmark suite for Cobalt sorting algorithms (PDQSort and Adaptive Merge Sort)
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/algorithm/sort.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_ELEMENTS 100000
#define NUM_RUNS 5

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int compare_int(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

static bool is_sorted(const int *arr, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) {
            return false;
        }
    }
    return true;
}

typedef void (*sort_func_t)(void *base, size_t nmemb, size_t size, compare_func_t compar);

static void run_benchmark(const char *title,
                          sort_func_t sort_fn,
                          const int  *template_arr,
                          int        *work_arr,
                          size_t      n,
                          int         num_runs)
{
    double total_ms = 0.0;
    for (int r = 0; r < num_runs; r++) {
        memcpy(work_arr, template_arr, n * sizeof(int));
        double start = current_time_ms();
        sort_fn(work_arr, n, sizeof(int), compare_int);
        double elapsed = current_time_ms() - start;
        total_ms += elapsed;
        if (!is_sorted(work_arr, n)) {
            fprintf(stderr, "Validation failed: %s is not properly sorted\n", title);
            exit(1);
        }
    }

    double avg_ms            = total_ms / (double)num_runs;
    double total_elements    = (double)n * (double)num_runs;
    double total_bytes       = total_elements * (double)sizeof(int);
    double sec               = total_ms / 1000.0;
    double throughput_elem_s = (sec > 0.0) ? (total_elements / sec) / 1e6 : 0.0;
    double throughput_mb_s   = (sec > 0.0) ? (total_bytes / (1024.0 * 1024.0)) / sec : 0.0;

    printf("%s\n", title);
    printf("   Time: %.3f ms (avg of %d runs) | Throughput: %.2f M elem/s (%.2f MB/s)\n\n",
           avg_ms,
           num_runs,
           throughput_elem_s,
           throughput_mb_s);
}

int main(void)
{
    printf("Cobalt Sorting Benchmark Suite (N = %d integers)\n", NUM_ELEMENTS);
    printf("==================================================\n\n");

    int *template_arr = (int *)malloc(NUM_ELEMENTS * sizeof(int));
    int *work_arr     = (int *)malloc(NUM_ELEMENTS * sizeof(int));
    if (!template_arr || !work_arr) {
        fprintf(stderr, "Failed to allocate memory for benchmark arrays\n");
        free(template_arr);
        free(work_arr);
        return 1;
    }

    /* 1. cobalt_qsort - Random distribution */
    uint32_t seed = 123456789U;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        seed            = seed * 1664525U + 1013904223U;
        template_arr[i] = (int)(seed & 0x7FFFFFFF);
    }
    run_benchmark("1. cobalt_qsort - Random distribution",
                  cobalt_qsort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    /* 2. cobalt_qsort - Pre-sorted distribution (O(N)) */
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        template_arr[i] = i;
    }
    run_benchmark("2. cobalt_qsort - Pre-sorted distribution (O(N))",
                  cobalt_qsort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    /* 3. cobalt_qsort - Reverse-sorted distribution */
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        template_arr[i] = NUM_ELEMENTS - 1 - i;
    }
    run_benchmark("3. cobalt_qsort - Reverse-sorted distribution",
                  cobalt_qsort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    /* 4. cobalt_qsort - Low-cardinality duplicates (5 distinct keys, O(N) DNF) */
    seed = 987654321U;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        seed            = seed * 1664525U + 1013904223U;
        template_arr[i] = (int)(seed % 5);
    }
    run_benchmark("4. cobalt_qsort - Low-cardinality duplicates (5 distinct keys, O(N) DNF)",
                  cobalt_qsort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    /* 5. cobalt_stable_sort - Random distribution */
    seed = 123456789U;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        seed            = seed * 1664525U + 1013904223U;
        template_arr[i] = (int)(seed & 0x7FFFFFFF);
    }
    run_benchmark("5. cobalt_stable_sort - Random distribution",
                  cobalt_stable_sort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    /* 6. cobalt_stable_sort - Pre-sorted distribution (O(N)) */
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        template_arr[i] = i;
    }
    run_benchmark("6. cobalt_stable_sort - Pre-sorted distribution (O(N))",
                  cobalt_stable_sort,
                  template_arr,
                  work_arr,
                  NUM_ELEMENTS,
                  NUM_RUNS);

    free(template_arr);
    free(work_arr);

    printf("Benchmark completed\n");
    return 0;
}
