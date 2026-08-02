#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

char *cobalt_strdup(const char *s)
{
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s) + 1;
    char  *dup = malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }

    return dup;
}
