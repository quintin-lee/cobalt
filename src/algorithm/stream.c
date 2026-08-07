/**
 * @file stream.c
 * @brief Stream processing utilities implementation
 */

#include "cobalt/algorithm/stream.h"
#include <string.h>

void cobalt_stream_take(const void *input, void *output, size_t n, size_t nmemb, size_t size)
{
    if (!input || !output || size == 0 || nmemb == 0) {
        return;
    }

    size_t count = n < nmemb ? n : nmemb;
    memcpy(output, input, count * size);
}

void cobalt_stream_drop(
    const void *input, void *output, size_t n, size_t nmemb, size_t size, size_t *out_nemb)
{
    if (!input || !output || size == 0 || nmemb == 0) {
        if (out_nemb) {
            *out_nemb = 0;
        }
        return;
    }

    size_t skip  = n < nmemb ? n : nmemb;
    size_t count = nmemb - skip;

    if (count > 0) {
        memcpy(output, (const char *)input + skip * size, count * size);
    }

    if (out_nemb) {
        *out_nemb = count;
    }
}

void cobalt_stream_take_while(
    const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred)
{
    if (!input || !output || size == 0 || !pred) {
        if (nmemb) {
            *nmemb = 0;
        }
        return;
    }

    size_t count = *nmemb;
    size_t i     = 0;

    for (i = 0; i < count; i++) {
        if (!pred((const char *)input + i * size)) {
            break;
        }
    }

    if (i > 0) {
        memcpy(output, input, i * size);
    }

    if (nmemb) {
        *nmemb = i;
    }
}

void cobalt_stream_drop_while(
    const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred)
{
    if (!input || !output || size == 0 || !pred) {
        if (nmemb) {
            *nmemb = 0;
        }
        return;
    }

    size_t count = *nmemb;
    size_t skip  = 0;

    for (skip = 0; skip < count; skip++) {
        if (!pred((const char *)input + skip * size)) {
            break;
        }
    }

    size_t remaining = count - skip;
    if (remaining > 0) {
        memcpy(output, (const char *)input + skip * size, remaining * size);
    }

    if (nmemb) {
        *nmemb = remaining;
    }
}
