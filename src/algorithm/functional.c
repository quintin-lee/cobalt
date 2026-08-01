#include "cobalt/algorithm/functional.h"
#include <stddef.h>

int predicate_equal(const void* a, const void* b, compare_func_t comp)
{
    if (!a || !b)
        return a == b;
    return comp(a, b) == 0;
}

int predicate_not_equal(const void* a, const void* b, compare_func_t comp)
{
    return !predicate_equal(a, b, comp);
}

int predicate_null(const void* item)
{
    return item == NULL;
}

int predicate_nonnull(const void* item)
{
    return item != NULL;
}
