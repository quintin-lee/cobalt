/**
 * @file benchmark_json.c
 * @brief Benchmark suite for Cobalt JSON engine (SAX, DOM with Arena, and Serialization)
 */

#include "cobalt/memory/arena.h"
#include "cobalt/module/json.h"
#include "cobalt/module/json_sax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* Empty SAX handler measuring pure streaming scan speed */
static bool noop_null(void *ctx)
{
    (void)ctx;
    return true;
}
static bool noop_bool(void *ctx, bool val)
{
    (void)ctx;
    (void)val;
    return true;
}
static bool noop_number(void *ctx, double val, const char *raw, size_t len)
{
    (void)ctx;
    (void)val;
    (void)raw;
    (void)len;
    return true;
}
static bool noop_string(void *ctx, const char *val, size_t len)
{
    (void)ctx;
    (void)val;
    (void)len;
    return true;
}
static bool noop_key(void *ctx, const char *key, size_t len)
{
    (void)ctx;
    (void)key;
    (void)len;
    return true;
}
static bool noop_container(void *ctx)
{
    (void)ctx;
    return true;
}

static const cobalt_json_sax_handler_t noop_sax_handler = {
    .on_null         = noop_null,
    .on_bool         = noop_bool,
    .on_number       = noop_number,
    .on_string       = noop_string,
    .on_key          = noop_key,
    .on_start_object = noop_container,
    .on_end_object   = noop_container,
    .on_start_array  = noop_container,
    .on_end_array    = noop_container,
};

static char *generate_sample_json(size_t num_items, size_t *out_len)
{
    size_t cap = num_items * 160 + 256;
    char  *buf = (char *)malloc(cap);
    if (!buf) {
        return NULL;
    }

    size_t len = 0;
    len +=
        snprintf(buf + len, cap - len, "{\"status\":\"ok\",\"total\":%zu,\"items\":[", num_items);
    for (size_t i = 0; i < num_items; i++) {
        if (i > 0) {
            buf[len++] = ',';
        }
        len += snprintf(
            buf + len,
            cap - len,
            "{\"id\":%zu,\"name\":\"item_%zu\",\"score\":%.2f,\"active\":%s,\"meta\":null}",
            i,
            i,
            (double)i * 1.5,
            (i % 2 == 0) ? "true" : "false");
    }
    len += snprintf(buf + len, cap - len, "]}");
    *out_len = len;
    return buf;
}

int main(void)
{
    printf("JSON Engine Benchmark\n");
    printf("=====================\n");

    size_t doc_len = 0;
    char  *doc     = generate_sample_json(1000, &doc_len);
    if (!doc) {
        fprintf(stderr, "Failed to allocate benchmark payload\n");
        return 1;
    }

    double doc_mb = (double)doc_len / (1024.0 * 1024.0);
    printf("Payload: %zu bytes (%.2f KB, 1,000 objects)\n\n", doc_len, (double)doc_len / 1024.0);

    /* Test 1: Streaming SAX Parsing */
    const int sax_iters = 1000;
    double    start     = current_time_ms();
    for (int i = 0; i < sax_iters; i++) {
        cobalt_json_sax_parse(doc, doc_len, &noop_sax_handler, NULL, NULL);
    }
    double elapsed_sax   = current_time_ms() - start;
    double total_sax_mb  = doc_mb * sax_iters;
    double sax_mb_per_s  = total_sax_mb / (elapsed_sax / 1000.0);
    double sax_ops_per_s = (double)sax_iters / (elapsed_sax / 1000.0);
    printf("1. Streaming SAX Parser:\n");
    printf("   Throughput: %.2f MB/s (%.1f ops/sec, total %.2f ms)\n\n",
           sax_mb_per_s,
           sax_ops_per_s,
           elapsed_sax);

    /* Test 2: DOM Parsing with Internal Arena */
    const int dom_iters = 500;
    start               = current_time_ms();
    for (int i = 0; i < dom_iters; i++) {
        json_node_t *root = json_parse(doc);
        json_destroy(root);
    }
    double elapsed_dom   = current_time_ms() - start;
    double total_dom_mb  = doc_mb * dom_iters;
    double dom_mb_per_s  = total_dom_mb / (elapsed_dom / 1000.0);
    double dom_ops_per_s = (double)dom_iters / (elapsed_dom / 1000.0);
    printf("2. DOM Parser (Internal Arena + O(1) Destroy):\n");
    printf("   Throughput: %.2f MB/s (%.1f ops/sec, total %.2f ms)\n\n",
           dom_mb_per_s,
           dom_ops_per_s,
           elapsed_dom);

    /* Test 3: DOM Parsing with Reused External Arena */
    cobalt_arena_t *arena = cobalt_arena_create(64 * 1024);
    start                 = current_time_ms();
    for (int i = 0; i < dom_iters; i++) {
        json_node_t *root = json_parse_with_arena(doc, arena);
        (void)root;
        cobalt_arena_reset(arena);
    }
    double elapsed_arena_dom = current_time_ms() - start;
    double arena_dom_mb_s    = total_dom_mb / (elapsed_arena_dom / 1000.0);
    double arena_dom_ops_s   = (double)dom_iters / (elapsed_arena_dom / 1000.0);
    cobalt_arena_destroy(arena);
    printf("3. DOM Parser (Reused External Arena + Reset):\n");
    printf("   Throughput: %.2f MB/s (%.1f ops/sec, total %.2f ms)\n\n",
           arena_dom_mb_s,
           arena_dom_ops_s,
           elapsed_arena_dom);

    /* Test 4: Serialization */
    json_node_t *sample_tree = json_parse(doc);
    const int    ser_iters   = 500;
    start                    = current_time_ms();
    for (int i = 0; i < ser_iters; i++) {
        char *str = json_serialize(sample_tree);
        free(str);
    }
    double elapsed_ser   = current_time_ms() - start;
    double total_ser_mb  = doc_mb * ser_iters;
    double ser_mb_per_s  = total_ser_mb / (elapsed_ser / 1000.0);
    double ser_ops_per_s = (double)ser_iters / (elapsed_ser / 1000.0);
    json_destroy(sample_tree);
    printf("4. Optimized Serializer:\n");
    printf("   Throughput: %.2f MB/s (%.1f ops/sec, total %.2f ms)\n\n",
           ser_mb_per_s,
           ser_ops_per_s,
           elapsed_ser);

    free(doc);
    printf("Benchmark completed\n");
    return 0;
}
