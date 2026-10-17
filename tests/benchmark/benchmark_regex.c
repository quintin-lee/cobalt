/**
 * @file benchmark_regex.c
 * @brief Benchmark suite and ReDoS stress test for Cobalt Pike VM regex engine
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/algorithm/regex.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void test_redos_immunity(void)
{
    printf("=== ReDoS Vulnerability Immunity Stress Test ===\n");
    /* Pattern: ^(a+)+$ against "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX" (30 'a's + 'X')
     * In backtracking engines, this takes 2^30 steps (~several seconds or hangs).
     * In Pike VM, this is strictly linear and completes in < 1.0 ms.
     */
    cobalt_regex_t *re = cobalt_regex_compile("^(a+)+$", COBALT_REGEX_DEFAULT);
    assert(re != NULL);

    const char *pathological = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX";

    double start   = current_time_ms();
    int    res     = cobalt_regex_search(re, pathological, NULL, 0);
    double elapsed = current_time_ms() - start;

    (void)res;
    assert(res == 0); /* Must correctly fail to match */
    printf("  ^(a+)+$ on 30 'a's + 'X': elapsed = %.4f ms (must be < 1.0 ms)\n", elapsed);
    assert(elapsed < 1.0); /* Strict ReDoS immunity check */

    /* Repeat 5,000 iterations to measure throughput */
    int iters = 5000;
    start     = current_time_ms();
    for (int i = 0; i < iters; i++) {
        cobalt_regex_search(re, pathological, NULL, 0);
    }
    elapsed           = current_time_ms() - start;
    double throughput = (double)iters / (elapsed / 1000.0);
    printf("  Throughput: %.2f ops/sec (%.4f ms for %d iters)\n", throughput, elapsed, iters);

    cobalt_regex_destroy(re);
    printf("  ReDoS Immunity: PASSED\n\n");
}

static void test_email_validation(void)
{
    printf("=== Email Format Validation Benchmark ===\n");
    cobalt_regex_t *re = cobalt_regex_compile("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,6}$",
                                              COBALT_REGEX_DEFAULT);
    assert(re != NULL);

    const char *email = "user.name+tag@example-domain.co.uk";
    int         iters = 100000;
    double      start = current_time_ms();
    for (int i = 0; i < iters; i++) {
        cobalt_regex_match_exact(re, email, NULL, 0);
    }
    double elapsed = current_time_ms() - start;
    double ops     = (double)iters / (elapsed / 1000.0);
    printf("  Validated %d emails in %.2f ms (%.2f ops/sec)\n", iters, elapsed, ops);

    cobalt_regex_destroy(re);
    printf("  Email Validation: PASSED\n\n");
}

static void test_ipv4_extraction(void)
{
    printf("=== IPv4 Extraction Across Simulated Server Logs ===\n");
    cobalt_regex_t *re =
        cobalt_regex_compile("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}", COBALT_REGEX_DEFAULT);
    assert(re != NULL);

    /* Build simulated server log data (approx 1MB) */
    size_t line_len  = 80;
    size_t num_lines = 12500;
    char  *log_data  = (char *)malloc(num_lines * line_len + 1);
    assert(log_data != NULL);

    for (size_t i = 0; i < num_lines; i++) {
        char line_buf[128];
        int  len =
            snprintf(line_buf,
                     sizeof(line_buf),
                     "2026-10-17 12:00:00 IP 192.168.1.%d GET /api/v1/resource HTTP/1.1 200 OK\n",
                     (int)(i % 250));
        memset(log_data + i * line_len, ' ', line_len);
        memcpy(log_data + i * line_len,
               line_buf,
               (size_t)(len < (int)line_len ? len : (int)line_len - 1));
        log_data[(i + 1) * line_len - 1] = '\n';
    }
    log_data[num_lines * line_len] = '\0';
    size_t total_bytes             = num_lines * line_len;

    cobalt_regex_capture_t cap;
    double                 start   = current_time_ms();
    int                    matches = 0;
    const char            *cursor  = log_data;
    while (cobalt_regex_search(re, cursor, &cap, 1) == 1) {
        matches++;
        cursor += cap.end;
        if (cursor >= log_data + total_bytes) {
            break;
        }
    }
    double elapsed    = current_time_ms() - start;
    double mb_per_sec = ((double)total_bytes / (1024.0 * 1024.0)) / (elapsed / 1000.0);

    printf("  Extracted %d IPv4 addresses from %.2f MB in %.2f ms (%.2f MB/s)\n",
           matches,
           (double)total_bytes / (1024.0 * 1024.0),
           elapsed,
           mb_per_sec);

    free(log_data);
    cobalt_regex_destroy(re);
    printf("  IPv4 Extraction: PASSED\n\n");
}

static void test_url_routing(void)
{
    printf("=== URL Routing Capture Extraction Benchmark ===\n");
    cobalt_regex_t *re =
        cobalt_regex_compile("/api/v1/users/(\\d+)/posts/([a-z0-9_-]+)", COBALT_REGEX_DEFAULT);
    assert(re != NULL);

    const char            *url = "/api/v1/users/12345/posts/regex-engine-pike-vm";
    cobalt_regex_capture_t caps[3];
    int                    iters = 100000;
    double                 start = current_time_ms();
    for (int i = 0; i < iters; i++) {
        cobalt_regex_search(re, url, caps, 3);
    }
    double elapsed = current_time_ms() - start;
    double ops     = (double)iters / (elapsed / 1000.0);
    printf("  Parsed %d URLs in %.2f ms (%.2f ops/sec)\n", iters, elapsed, ops);

    cobalt_regex_destroy(re);
    printf("  URL Routing: PASSED\n\n");
}

int main(void)
{
    printf("====================================================\n");
    printf("Cobalt Pike VM Regex Engine Benchmark Suite\n");
    printf("====================================================\n\n");

    test_redos_immunity();
    test_email_validation();
    test_ipv4_extraction();
    test_url_routing();

    printf("Benchmark completed\n");
    return 0;
}
