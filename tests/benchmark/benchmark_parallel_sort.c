/**
 * @file benchmark_parallel_sort.c
 * @brief Benchmark suite for parallel sort algorithms against sequential baselines
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/algorithm/sort.h"
#include "cobalt/module/parallel_sort.h"
#include "cobalt/module/threadpool.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_SORT_N 1000000
#define BENCH_RUNS 3

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

int main(void)
{
    printf("=================================================================\n");
    printf("Cobalt Parallel Sort Benchmark (N = %d integers)\n", BENCH_SORT_N);
    printf("=================================================================\n\n");

    int *template_arr = (int *)malloc(BENCH_SORT_N * sizeof(int));
    int *work_arr     = (int *)malloc(BENCH_SORT_N * sizeof(int));
    if (!template_arr || !work_arr) {
        fprintf(stderr, "Allocation failure\n");
        free(template_arr);
        free(work_arr);
        return 1;
    }

    /* Seed pseudo-random generator */
    uint32_t seed = 123456789U;
    for (size_t i = 0; i < BENCH_SORT_N; i++) {
        seed            = seed * 1664525U + 1013904223U;
        template_arr[i] = (int)(seed & 0x7FFFFFFFU);
    }

    /* ---------------------------------------------------------------------- */
    /* 1. Sequential PDQSort Baseline                                         */
    /* ---------------------------------------------------------------------- */
    double seq_qsort_ms = 0.0;
    for (int r = 0; r < BENCH_RUNS; r++) {
        memcpy(work_arr, template_arr, BENCH_SORT_N * sizeof(int));
        double t0 = current_time_ms();
        cobalt_qsort(work_arr, BENCH_SORT_N, sizeof(int), compare_int);
        seq_qsort_ms += (current_time_ms() - t0);
        if (!is_sorted(work_arr, BENCH_SORT_N)) {
            fprintf(stderr, "Sequential qsort validation failed!\n");
            return 1;
        }
    }
    seq_qsort_ms /= (double)BENCH_RUNS;
    printf("1. Sequential cobalt_qsort:\n");
    printf("   Time: %.2f ms | Throughput: %.2f M elem/s\n\n",
           seq_qsort_ms,
           ((double)BENCH_SORT_N / (seq_qsort_ms / 1000.0)) / 1e6);

    /* ---------------------------------------------------------------------- */
    /* 2. Parallel PDQSort (2, 4, 8 threads)                                  */
    /* ---------------------------------------------------------------------- */
    size_t thread_counts[] = {2, 4, 8};
    for (size_t t = 0; t < sizeof(thread_counts) / sizeof(thread_counts[0]); t++) {
        size_t               threads = thread_counts[t];
        cobalt_threadpool_t *pool    = cobalt_threadpool_create(threads);
        if (!pool) {
            continue;
        }

        double par_qsort_ms = 0.0;
        for (int r = 0; r < BENCH_RUNS; r++) {
            memcpy(work_arr, template_arr, BENCH_SORT_N * sizeof(int));
            double t0 = current_time_ms();
            cobalt_parallel_qsort(pool, work_arr, BENCH_SORT_N, sizeof(int), compare_int);
            par_qsort_ms += (current_time_ms() - t0);
            if (!is_sorted(work_arr, BENCH_SORT_N)) {
                fprintf(stderr, "Parallel qsort validation failed!\n");
                return 1;
            }
        }
        par_qsort_ms /= (double)BENCH_RUNS;
        double speedup = seq_qsort_ms / par_qsort_ms;

        printf("2.%zu Parallel cobalt_parallel_qsort (%zu threads):\n", t + 1, threads);
        printf("   Time: %.2f ms | Throughput: %.2f M elem/s | Speedup: %.2fx\n\n",
               par_qsort_ms,
               ((double)BENCH_SORT_N / (par_qsort_ms / 1000.0)) / 1e6,
               speedup);

        cobalt_threadpool_destroy(pool);
    }

    /* ---------------------------------------------------------------------- */
    /* 3. Sequential Stable Sort Baseline                                     */
    /* ---------------------------------------------------------------------- */
    double seq_stable_ms = 0.0;
    for (int r = 0; r < BENCH_RUNS; r++) {
        memcpy(work_arr, template_arr, BENCH_SORT_N * sizeof(int));
        double t0 = current_time_ms();
        cobalt_stable_sort(work_arr, BENCH_SORT_N, sizeof(int), compare_int);
        seq_stable_ms += (current_time_ms() - t0);
        if (!is_sorted(work_arr, BENCH_SORT_N)) {
            fprintf(stderr, "Sequential stable_sort validation failed!\n");
            return 1;
        }
    }
    seq_stable_ms /= (double)BENCH_RUNS;
    printf("3. Sequential cobalt_stable_sort:\n");
    printf("   Time: %.2f ms | Throughput: %.2f M elem/s\n\n",
           seq_stable_ms,
           ((double)BENCH_SORT_N / (seq_stable_ms / 1000.0)) / 1e6);

    /* ---------------------------------------------------------------------- */
    /* 4. Parallel Stable Sort (2, 4, 8 threads)                              */
    /* ---------------------------------------------------------------------- */
    for (size_t t = 0; t < sizeof(thread_counts) / sizeof(thread_counts[0]); t++) {
        size_t               threads = thread_counts[t];
        cobalt_threadpool_t *pool    = cobalt_threadpool_create(threads);
        if (!pool) {
            continue;
        }

        double par_stable_ms = 0.0;
        for (int r = 0; r < BENCH_RUNS; r++) {
            memcpy(work_arr, template_arr, BENCH_SORT_N * sizeof(int));
            double t0 = current_time_ms();
            cobalt_parallel_stable_sort(pool, work_arr, BENCH_SORT_N, sizeof(int), compare_int);
            par_stable_ms += (current_time_ms() - t0);
            if (!is_sorted(work_arr, BENCH_SORT_N)) {
                fprintf(stderr, "Parallel stable_sort validation failed!\n");
                return 1;
            }
        }
        par_stable_ms /= (double)BENCH_RUNS;
        double speedup = seq_stable_ms / par_stable_ms;

        printf("4.%zu Parallel cobalt_parallel_stable_sort (%zu threads):\n", t + 1, threads);
        printf("   Time: %.2f ms | Throughput: %.2f M elem/s | Speedup: %.2fx\n\n",
               par_stable_ms,
               ((double)BENCH_SORT_N / (par_stable_ms / 1000.0)) / 1e6,
               speedup);

        cobalt_threadpool_destroy(pool);
    }

    free(template_arr);
    free(work_arr);

    printf("Benchmark completed\n");
    return 0;
}
