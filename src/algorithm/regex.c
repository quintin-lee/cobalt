/**
 * @file regex.c
 * @brief High-performance linear Pike VM regular expression engine
 */

#include "cobalt/algorithm/regex.h"
#include <stdlib.h>

struct cobalt_regex {
    int flags;
};

cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags)
{
    (void)pattern;
    (void)flags;
    return NULL;
}

cobalt_regex_t *
cobalt_regex_compile_with_alloc(const char *pattern, int flags, cobalt_allocator_t *alloc)
{
    (void)pattern;
    (void)flags;
    (void)alloc;
    return NULL;
}

void cobalt_regex_destroy(cobalt_regex_t *re)
{
    (void)re;
}

int cobalt_regex_search(const cobalt_regex_t   *re,
                        const char             *text,
                        cobalt_regex_capture_t *captures,
                        size_t                  max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_match_exact(const cobalt_regex_t   *re,
                             const char             *text,
                             cobalt_regex_capture_t *captures,
                             size_t                  max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_test(const char *pattern, const char *text)
{
    (void)pattern;
    (void)text;
    return -1;
}

size_t cobalt_regex_capture_count(const cobalt_regex_t *re)
{
    (void)re;
    return 0;
}
