/**
 * @file benchmark_shm.c
 * @brief Benchmark suite for Layer 2 Shared Memory IPC Channel
 * @details Measures Round-Trip Latency (RTT) and Throughput across wait policies and payload sizes.
 */

#define _POSIX_C_SOURCE 199309L

#include "cobalt/module/shm_channel.h"
#include "cobalt/platform/shm.h"
#include "cobalt/platform/thread.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static inline uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t ua = *(const uint64_t *)a;
    uint64_t ub = *(const uint64_t *)b;
    return (ua < ub) ? -1 : (ua > ub) ? 1 : 0;
}

static void cleanup_shm_channel(const char *name)
{
    if (!name) {
        return;
    }
    cobalt_shm_unlink(name);
#ifndef _WIN32
    char        path[256];
    const char *p = name;
    while (*p == '/') {
        p++;
    }
    snprintf(path, sizeof(path), "/dev/shm/%s_notif", p);
    unlink(path);
    snprintf(path, sizeof(path), "/tmp/%s_notif", p);
    unlink(path);
#endif
}

/* ========================================================================= */
/* 1. Round-Trip Latency (RTT) Benchmarks                                    */
/* ========================================================================= */

typedef struct {
    const char              *req_name;
    const char              *rsp_name;
    cobalt_shm_mode_t        mode;
    cobalt_shm_wait_policy_t policy;
    size_t                   capacity;
    size_t                   slot_size;
    int                      total_rounds;
} rtt_pong_arg_t;

static void *rtt_pong_worker(void *arg)
{
    rtt_pong_arg_t *p = (rtt_pong_arg_t *)arg;

    cobalt_shm_channel_t *req_rx = cobalt_shm_channel_open(p->req_name, p->policy);
    cobalt_shm_channel_t *rsp_tx = cobalt_shm_channel_open(p->rsp_name, p->policy);
    if (!req_rx || !rsp_tx) {
        fprintf(stderr,
                "rtt_pong_worker: open failed! req_rx=%p rsp_tx=%p\n",
                (void *)req_rx,
                (void *)rsp_tx);
        return NULL;
    }

    uint64_t buf     = 0;
    size_t   out_len = 0;
    uint16_t type    = 0;

    for (int i = 0; i < p->total_rounds; i++) {
        int rc = cobalt_shm_channel_recv(req_rx, &type, &buf, sizeof(buf), &out_len, -1);
        if (rc != COBALT_SUCCESS) {
            fprintf(stderr, "rtt_pong_worker: recv failed! rc=%d at i=%d\n", rc, i);
            break;
        }

        rc = cobalt_shm_channel_send(rsp_tx, type, &buf, sizeof(buf), -1);
        if (rc != COBALT_SUCCESS) {
            fprintf(stderr, "rtt_pong_worker: send failed! rc=%d at i=%d\n", rc, i);
            break;
        }
    }

    cobalt_shm_channel_destroy(req_rx);
    cobalt_shm_channel_destroy(rsp_tx);
    return NULL;
}

static void bench_rtt(const char              *title,
                      cobalt_shm_mode_t        mode,
                      cobalt_shm_wait_policy_t policy,
                      int                      iterations)
{
    printf("=== Round-Trip Latency: %s (%d rounds) ===\n", title, iterations);
    fflush(stdout);

    const char *req_name = "/cobalt_bench_rtt_req";
    const char *rsp_name = "/cobalt_bench_rtt_rsp";
    cleanup_shm_channel(req_name);
    cleanup_shm_channel(rsp_name);

    size_t capacity  = (mode == COBALT_SHM_MODE_SPSC) ? 65536 : 1024;
    size_t slot_size = (mode == COBALT_SHM_MODE_MPMC) ? 64 : 0;

    cobalt_shm_config_t req_cfg = {
        .name              = req_name,
        .mode              = mode,
        .wait_policy       = policy,
        .capacity          = capacity,
        .slot_size         = slot_size,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };
    cobalt_shm_config_t rsp_cfg = {
        .name              = rsp_name,
        .mode              = mode,
        .wait_policy       = policy,
        .capacity          = capacity,
        .slot_size         = slot_size,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *req_tx = cobalt_shm_channel_create(&req_cfg);
    cobalt_shm_channel_t *rsp_rx = cobalt_shm_channel_create(&rsp_cfg);
    if (!req_tx || !rsp_rx) {
        fprintf(stderr,
                "bench_rtt: create failed! req_tx=%p rsp_rx=%p\n",
                (void *)req_tx,
                (void *)rsp_rx);
        return;
    }

    int            warmup_rounds = (iterations >= 100) ? 100 : 0;
    rtt_pong_arg_t pong_arg      = {
        .req_name     = req_name,
        .rsp_name     = rsp_name,
        .mode         = mode,
        .policy       = policy,
        .capacity     = capacity,
        .slot_size    = slot_size,
        .total_rounds = warmup_rounds + iterations,
    };

    cobalt_thread_t pong_thread;
    int             trc = cobalt_thread_create(rtt_pong_worker, &pong_arg, &pong_thread);
    (void)trc;
    assert(trc == 0);

    uint64_t *latencies = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)iterations);
    assert(latencies != NULL);

    uint64_t sum_ns  = 0;
    uint64_t payload = 0xCAFEBABEULL;
    uint64_t rx_buf  = 0;
    size_t   out_len = 0;
    uint16_t type    = 0;

    /* Warm-up rounds */
    for (int i = 0; i < warmup_rounds; i++) {
        int s_rc = cobalt_shm_channel_send(req_tx, 1, &payload, sizeof(payload), -1);
        (void)s_rc;
        int r_rc = cobalt_shm_channel_recv(rsp_rx, &type, &rx_buf, sizeof(rx_buf), &out_len, -1);
        (void)r_rc;
    }

    for (int i = 0; i < iterations; i++) {
        uint64_t t0 = get_time_ns();
        int rc = cobalt_shm_channel_send(req_tx, (uint16_t)(i + 1), &payload, sizeof(payload), -1);
        (void)rc;
        assert(rc == COBALT_SUCCESS);

        rc = cobalt_shm_channel_recv(rsp_rx, &type, &rx_buf, sizeof(rx_buf), &out_len, -1);
        (void)rc;
        assert(rc == COBALT_SUCCESS);
        uint64_t t1 = get_time_ns();

        uint64_t rtt = (t1 > t0) ? (t1 - t0) : 0;
        latencies[i] = rtt;
        sum_ns += rtt;
    }

    cobalt_thread_join(pong_thread);

    qsort(latencies, (size_t)iterations, sizeof(uint64_t), cmp_u64);

    uint64_t min_ns = latencies[0];
    uint64_t p50_ns = latencies[(size_t)iterations * 50 / 100];
    uint64_t p90_ns = latencies[(size_t)iterations * 90 / 100];
    uint64_t p99_ns = latencies[(size_t)iterations * 99 / 100];
    uint64_t max_ns = latencies[(size_t)iterations - 1];
    double   avg_ns = (double)sum_ns / (double)iterations;

    printf("  Min Latency:    %" PRIu64 " ns\n", min_ns);
    printf("  Median (P50):   %" PRIu64 " ns\n", p50_ns);
    printf("  P90 Latency:    %" PRIu64 " ns\n", p90_ns);
    printf("  P99 Latency:    %" PRIu64 " ns\n", p99_ns);
    printf("  Max Latency:    %" PRIu64 " ns\n", max_ns);
    printf("  Average:        %.2f ns\n", avg_ns);

    if (policy == COBALT_SHM_WAIT_SPIN) {
        printf("  Target (<= 80ns RTT): %s (Median = %" PRIu64 " ns)\n",
               p50_ns <= 80 ? "ACHIEVED" : "MARGINAL",
               p50_ns);
    }
    printf("\n");

    free(latencies);
    cobalt_shm_channel_destroy(req_tx);
    cobalt_shm_channel_destroy(rsp_rx);
    cleanup_shm_channel(req_name);
    cleanup_shm_channel(rsp_name);
}

/* ========================================================================= */
/* 2. Throughput Benchmarks                                                  */
/* ========================================================================= */

typedef struct {
    const char *name;
    size_t      payload_size;
    int         num_messages;
} tput_consumer_arg_t;

static void *tput_consumer_worker(void *arg)
{
    tput_consumer_arg_t *c = (tput_consumer_arg_t *)arg;

    cobalt_shm_channel_t *chan = cobalt_shm_channel_open(c->name, COBALT_SHM_WAIT_BLOCKING);
    assert(chan != NULL);

    uint8_t *buf = (uint8_t *)malloc(c->payload_size + 64);
    assert(buf != NULL);

    int received = 0;
    while (received < c->num_messages) {
        uint16_t    type = 0;
        const void *ptr  = NULL;
        size_t      len  = 0;

        int rc = cobalt_shm_channel_peek(chan, &type, &ptr, &len);
        if (rc == COBALT_SUCCESS) {
            cobalt_shm_channel_consume(chan);
            received++;
        } else {
            /* Fall back to recv with wait */
            size_t out_len = 0;
            rc = cobalt_shm_channel_recv(chan, &type, buf, c->payload_size + 64, &out_len, 100);
            if (rc == COBALT_SUCCESS) {
                received++;
            }
        }
    }

    free(buf);
    cobalt_shm_channel_destroy(chan);
    return NULL;
}

static void bench_throughput(size_t payload_size, int num_messages)
{
    printf("=== Streaming Throughput: %zu Bytes Payload (%d messages) ===\n",
           payload_size,
           num_messages);

    const char *name = "/cobalt_bench_tput";
    cleanup_shm_channel(name);

    /* Allocate capacity large enough to buffer batches (min 1MB, power of 2) */
    size_t cap = 1048576;
    while (cap < payload_size * 16) {
        cap *= 2;
    }

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_ADAPTIVE,
        .capacity          = cap,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 2000,
    };

    cobalt_shm_channel_t *chan = cobalt_shm_channel_create(&cfg);
    assert(chan != NULL);

    tput_consumer_arg_t c_arg = {
        .name         = name,
        .payload_size = payload_size,
        .num_messages = num_messages,
    };

    cobalt_thread_t consumer_thread;
    int             trc = cobalt_thread_create(tput_consumer_worker, &c_arg, &consumer_thread);
    (void)trc;
    assert(trc == 0);

    /* Producer writes messages using zero-copy reserve & commit */
    double t_start = current_time_ms();

    for (int i = 0; i < num_messages; i++) {
        void *wptr    = NULL;
        int   rc      = 0;
        int   retries = 0;

        while ((rc = cobalt_shm_channel_reserve(chan, payload_size, &wptr)) ==
               COBALT_ERROR_QUEUE_FULL) {
            if (++retries > 10000) {
                /* Let consumer make progress */
                cobalt_thread_yield();
                retries = 0;
            }
        }
        (void)rc;
        assert(rc == COBALT_SUCCESS);

        /* Write pattern byte */
        *(uint8_t *)wptr = (uint8_t)(i & 0xFF);

        rc = cobalt_shm_channel_commit(chan, 1, payload_size);
        (void)rc;
        assert(rc == COBALT_SUCCESS);
    }

    cobalt_thread_join(consumer_thread);
    double t_end = current_time_ms();

    double elapsed_ms     = t_end - t_start;
    double elapsed_sec    = elapsed_ms / 1000.0;
    double msgs_per_sec   = (double)num_messages / elapsed_sec;
    double m_msgs_per_sec = msgs_per_sec / 1000000.0;
    double total_bytes    = (double)num_messages * (double)payload_size;
    double gb_per_sec     = (total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_sec;

    printf("  Elapsed Time:   %.2f ms (%.3f s)\n", elapsed_ms, elapsed_sec);
    printf("  Throughput:     %.2f M msgs/sec\n", m_msgs_per_sec);
    printf("  Bandwidth:      %.3f GB/s\n", gb_per_sec);

    if (payload_size <= 64) {
        printf("  Target (>= 20M msgs/sec): %s (%.2f M msgs/sec)\n",
               m_msgs_per_sec >= 20.0 ? "ACHIEVED" : "RECORDED",
               m_msgs_per_sec);
    }
    printf("\n");

    cobalt_shm_channel_destroy(chan);
    cleanup_shm_channel(name);
}

/* ========================================================================= */
/* Main Entry Point                                                          */
/* ========================================================================= */

int main(void)
{
    printf("============================================================\n");
    printf("Cobalt Shared Memory IPC Channel Benchmark Suite\n");
    printf("============================================================\n\n");

    /* 1. Round-Trip Latency */
    bench_rtt("SPSC Spin Policy", COBALT_SHM_MODE_SPSC, COBALT_SHM_WAIT_SPIN, 100000);
    bench_rtt("SPSC Blocking Policy", COBALT_SHM_MODE_SPSC, COBALT_SHM_WAIT_BLOCKING, 10000);
    bench_rtt("MPMC Blocking Policy", COBALT_SHM_MODE_MPMC, COBALT_SHM_WAIT_BLOCKING, 10000);

    /* 2. Streaming Throughput */
    bench_throughput(64, 1000000);
    bench_throughput(1024, 200000);
    bench_throughput(65536, 20000);

    printf("Benchmark completed\n");
    return 0;
}
