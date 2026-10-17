#ifndef COBALT_ALGORITHM_REGEX_H
#define COBALT_ALGORITHM_REGEX_H

/**
 * @file regex.h
 * @brief High-performance linear Pike VM regular expression engine
 * @details Implements a pure C11 Thompson NFA / Pike VM regular expression compiler
 *          and execution engine. Guarantees O(N * M) linear time matching with zero
 *          ReDoS catastrophic backtracking, stack-allocated fast path for zero heap
 *          allocations on small patterns, zero-copy capture slices, and backreference matching.
 *
 * @defgroup Regex Regular Expressions
 * @ingroup Algorithms
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum supported capture groups including group 0 (full match) */
#define COBALT_REGEX_MAX_CAPTURES 16

/**
 * @brief Regular expression compilation flags (bitwise combination)
 */
typedef enum {
    COBALT_REGEX_DEFAULT          = 0,      /**< Default case-sensitive, single-line matching */
    COBALT_REGEX_CASE_INSENSITIVE = 1 << 0, /**< Case-insensitive matching (i) */
    COBALT_REGEX_DOTALL = 1 << 1,   /**< Dot '.' matches any character including newline '\n' (s) */
    COBALT_REGEX_MULTILINE = 1 << 2 /**< '^' and '$' match line beginnings and endings (m) */
} cobalt_regex_flags_t;

/**
 * @brief Zero-copy capture group boundary slice
 */
typedef struct {
    size_t start;  /**< Starting byte offset (0-indexed, inclusive) */
    size_t end;    /**< Ending byte offset (exclusive, [start, end)) */
    size_t length; /**< Length of matched substring (end - start) */
} cobalt_regex_capture_t;

/**
 * @brief Opaque compiled regular expression object
 */
typedef struct cobalt_regex cobalt_regex_t;

/* -------------------------------------------------------------------------- */
/* Lifecycle Management                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Compile a regular expression pattern using the system allocator
 * @param pattern Null-terminated regular expression pattern
 * @param flags Bitwise combination of cobalt_regex_flags_t
 * @return Compiled regex object pointer, or NULL on syntax error / out of memory
 */
cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags);

/**
 * @brief Compile a regular expression pattern using a custom allocator
 * @param pattern Null-terminated regular expression pattern
 * @param flags Bitwise combination of cobalt_regex_flags_t
 * @param alloc Custom memory allocator (falls back to system allocator if NULL)
 * @return Compiled regex object pointer, or NULL on failure
 */
cobalt_regex_t *
cobalt_regex_compile_with_alloc(const char *pattern, int flags, cobalt_allocator_t *alloc);

/**
 * @brief Destroy a compiled regex object and release all internal bytecode and resources
 * @param re Regex object to destroy. Safe with NULL.
 */
void cobalt_regex_destroy(cobalt_regex_t *re);

/* -------------------------------------------------------------------------- */
/* Search & Match Operations                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Search for the first match of the pattern in the target text
 * @param re Compiled regex object
 * @param text Null-terminated text to search
 * @param captures Output array to receive capture slices (captures[0] is full match,
 *                 captures[1..N] are submatches). May be NULL if captures are unneeded.
 * @param max_captures Capacity of the captures array
 * @return 1 on match found, 0 on no match, -1 on invalid argument or error
 */
int cobalt_regex_search(const cobalt_regex_t   *re,
                        const char             *text,
                        cobalt_regex_capture_t *captures,
                        size_t                  max_captures);

/**
 * @brief Check if the entire text strictly matches the pattern from start to end
 * @param re Compiled regex object
 * @param text Null-terminated text to match
 * @param captures Output array for capture slices (optional, can be NULL)
 * @param max_captures Capacity of the captures array
 * @return 1 on exact match, 0 on no match, -1 on invalid argument or error
 */
int cobalt_regex_match_exact(const cobalt_regex_t   *re,
                             const char             *text,
                             cobalt_regex_capture_t *captures,
                             size_t                  max_captures);

/**
 * @brief Convenience one-liner to test if pattern matches anywhere in text
 * @param pattern Regular expression pattern
 * @param text Target text
 * @return 1 on match, 0 on no match, -1 on syntax error or invalid argument
 */
int cobalt_regex_test(const char *pattern, const char *text);

/**
 * @brief Get the number of capture groups defined in the pattern (excluding group 0)
 * @param re Compiled regex object
 * @return Number of capture groups
 */
size_t cobalt_regex_capture_count(const cobalt_regex_t *re);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_ALGORITHM_REGEX_H */
