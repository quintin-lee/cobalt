/**
 * @file debug_assert.h
 * @brief Debug assertion macros for development builds
 */

#ifndef COBALT_DEBUG_ASSERT_H
#define COBALT_DEBUG_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Assert macro that aborts on failure in debug builds
 * @param expr Expression to evaluate
 */
#ifdef COBALT_DEBUG
#define COBALT_ASSERT(expr)                                                                        \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(                                                                               \
                stderr, "Assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__);    \
            abort();                                                                               \
        }                                                                                          \
    } while (0)
#else
#define COBALT_ASSERT(expr) ((void)0)
#endif

#endif /* COBALT_DEBUG_ASSERT_H */
