#include "cobalt/container/lfskiplist.h"

struct cobalt_lfskiplist {
    int placeholder;
};

struct cobalt_lfskiplist_iter {
    int placeholder;
};

cobalt_lfskiplist_t *cobalt_lfskiplist_create(compare_func_t compar)
{
    (void)compar;
    return NULL;
}

cobalt_lfskiplist_t *cobalt_lfskiplist_create_with_allocator(compare_func_t      compar,
                                                             cobalt_allocator_t *alloc,
                                                             cobalt_ebr_t       *ebr)
{
    (void)compar;
    (void)alloc;
    (void)ebr;
    return NULL;
}

void cobalt_lfskiplist_destroy(cobalt_lfskiplist_t *skiplist)
{
    (void)skiplist;
}

int cobalt_lfskiplist_insert(cobalt_lfskiplist_t *skiplist, void *key, void *value)
{
    (void)skiplist;
    (void)key;
    (void)value;
    return -1;
}

int cobalt_lfskiplist_put(cobalt_lfskiplist_t *skiplist,
                          void                *key,
                          void                *value,
                          void               **out_old_value)
{
    (void)skiplist;
    (void)key;
    (void)value;
    (void)out_old_value;
    return -1;
}

int cobalt_lfskiplist_get(cobalt_lfskiplist_t *skiplist, const void *key, void **out_value)
{
    (void)skiplist;
    (void)key;
    (void)out_value;
    return -1;
}

bool cobalt_lfskiplist_contains(cobalt_lfskiplist_t *skiplist, const void *key)
{
    (void)skiplist;
    (void)key;
    return false;
}

int cobalt_lfskiplist_remove(cobalt_lfskiplist_t *skiplist, const void *key, void **out_old_value)
{
    (void)skiplist;
    (void)key;
    (void)out_old_value;
    return -1;
}

size_t cobalt_lfskiplist_size(const cobalt_lfskiplist_t *skiplist)
{
    (void)skiplist;
    return 0;
}

bool cobalt_lfskiplist_is_empty(const cobalt_lfskiplist_t *skiplist)
{
    (void)skiplist;
    return true;
}

cobalt_lfskiplist_iter_t *cobalt_lfskiplist_iter_create(cobalt_lfskiplist_t *skiplist,
                                                        const void          *min_key,
                                                        const void          *max_key)
{
    (void)skiplist;
    (void)min_key;
    (void)max_key;
    return NULL;
}

bool cobalt_lfskiplist_iter_next(cobalt_lfskiplist_iter_t *iter, void **out_key, void **out_value)
{
    (void)iter;
    (void)out_key;
    (void)out_value;
    return false;
}

void cobalt_lfskiplist_iter_destroy(cobalt_lfskiplist_iter_t *iter)
{
    (void)iter;
}

cobalt_lfskiplist_guard_t cobalt_lfskiplist_pin(cobalt_lfskiplist_t *skiplist)
{
    (void)skiplist;
    cobalt_lfskiplist_guard_t g = {0};
    return g;
}

void cobalt_lfskiplist_unpin(cobalt_lfskiplist_guard_t *guard)
{
    (void)guard;
}
