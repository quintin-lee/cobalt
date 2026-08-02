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
