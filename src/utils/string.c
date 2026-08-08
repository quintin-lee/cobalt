/**
 * @file string.c
 * @brief String utility module implementation
 */

#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief String duplication implementation
 * @details Calculates the length of the original string, allocates the corresponding memory, and
 * safely copies the contents into the new memory.
 */
char *cobalt_strdup(const char *s)
{
    // Return NULL safely if the input is null
    if (!s) {
        return NULL;
    }

    // Calculate required length (including the terminating '\0')
    size_t len = strlen(s) + 1;

    // Allocate new memory
    char *dup = malloc(len);
    if (dup) {
        // Copy all contents if allocation is successful
        memcpy(dup, s, len);
    }

    return dup;
}

int cobalt_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix) {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    if (prefix_len > strlen(str)) {
        return 0;
    }
    return strncmp(str, prefix, prefix_len) == 0;
}

int cobalt_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return 0;
    }
    size_t str_len    = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return 0;
    }
    return strcmp(str + (str_len - suffix_len), suffix) == 0;
}

int cobalt_contains(const char *str, const char *sub)
{
    if (!str || !sub) {
        return 0;
    }
    return strstr(str, sub) != NULL;
}
