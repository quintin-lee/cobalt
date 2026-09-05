/**
 * @file benchmark.c
 * @brief Lightweight benchmarking utilities implementation
 */

#include "cobalt/utils/benchmark.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @brief Sample the monotonic clock in milliseconds.
 *
 * @return Current monotonic time in milliseconds.
 */
static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

/**
 * @brief Time a callback over many iterations and summarize the results.
 *
 * @param name Benchmark label stored into the result.
 * @param iters Iteration count; zero returns 0.0 without measuring.
 * @param block Callback invoked once per iteration after a warmup run.
 * @param result Receives mean, stddev, min, max and iterations; may be NULL.
 * @return Mean iteration time in milliseconds, or 0.0 on invalid arguments.
 */
double
cobalt_bench_run(const char *name, size_t iters, void (*block)(void), cobalt_bench_result_t *result)
{
    if (!block || iters == 0) {
        return 0.0;
    }

    double *times = (double *)malloc(sizeof(double) * iters);
    if (!times) {
        fprintf(stderr, "ERROR: benchmark memory allocation failed\n");
        return 0.0;
    }

    /* Warmup: run once to prime caches */
    if (iters > 0) {
        block();
    }

    /* Measure */
    for (size_t i = 0; i < iters; i++) {
        double start = current_time_ms();
        block();
        times[i] = current_time_ms() - start;
    }

    /* Compute statistics */
    double sum     = 0.0;
    double min_val = times[0];
    double max_val = times[0];
    for (size_t i = 0; i < iters; i++) {
        sum += times[i];
        if (times[i] < min_val) {
            min_val = times[i];
        }
        if (times[i] > max_val) {
            max_val = times[i];
        }
    }
    double mean = sum / (double)iters;

    double sq_sum = 0.0;
    for (size_t i = 0; i < iters; i++) {
        double diff = times[i] - mean;
        sq_sum += diff * diff;
    }
    double stddev = sqrt(sq_sum / (double)iters);

    if (result) {
        result->name       = name;
        result->mean_ms    = mean;
        result->stddev_ms  = stddev;
        result->min_ms     = min_val;
        result->max_ms     = max_val;
        result->iterations = iters;
    }

    free(times);
    return mean;
}

/**
 * @brief Write NULL-terminated benchmark results as a JSON document.
 *
 * @param results Array terminated by a NULL name entry; NULL is ignored.
 * @param fp Stream receiving the JSON document; NULL is ignored.
 */
void cobalt_bench_output_json(const cobalt_bench_result_t *results, FILE *fp)
{
    if (!results || !fp) {
        return;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"benchmarks\": [\n");

    size_t count = 0;
    for (size_t i = 0; results[i].name != NULL; i++) {
        count++;
    }
    for (size_t i = 0; results[i].name != NULL; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", results[i].name);
        fprintf(fp, "      \"mean_ms\": %.6f,\n", results[i].mean_ms);
        fprintf(fp, "      \"stddev_ms\": %.6f,\n", results[i].stddev_ms);
        fprintf(fp, "      \"min_ms\": %.6f,\n", results[i].min_ms);
        fprintf(fp, "      \"max_ms\": %.6f,\n", results[i].max_ms);
        fprintf(fp, "      \"iterations\": %zu\n", results[i].iterations);
        fprintf(fp, "    }%s\n", (i < count - 1) ? "," : "");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
}

/**
 * @brief Print NULL-terminated benchmark results as an aligned table.
 *
 * @param results Array terminated by a NULL name entry; NULL is ignored.
 */
void cobalt_bench_print_results(const cobalt_bench_result_t *results)
{
    if (!results) {
        return;
    }

    printf("\n=== Benchmark Results ===\n");
    printf("%-40s %10s %10s %10s %10s %8s\n",
           "Benchmark",
           "Mean(ms)",
           "Stddev(ms)",
           "Min(ms)",
           "Max(ms)",
           "Iters");
    printf("%-40s %10s %10s %10s %10s %8s\n",
           "----------------------------------------",
           "----------",
           "----------",
           "----------",
           "----------",
           "--------");

    for (size_t i = 0; results[i].name != NULL; i++) {
        printf("%-40s %10.3f %10.3f %10.3f %10.3f %8zu\n",
               results[i].name,
               results[i].mean_ms,
               results[i].stddev_ms,
               results[i].min_ms,
               results[i].max_ms,
               results[i].iterations);
    }
    printf("=== End Results ===\n\n");
}
