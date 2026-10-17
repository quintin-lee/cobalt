#include "cobalt/memory/ebr.h"

struct cobalt_ebr {
    int placeholder;
};

struct cobalt_ebr_handle {
    int placeholder;
};

cobalt_ebr_t *cobalt_ebr_create(cobalt_allocator_t *alloc)
{
    (void)alloc;
    return NULL;
}

void cobalt_ebr_destroy(cobalt_ebr_t *ebr)
{
    (void)ebr;
}

cobalt_ebr_handle_t *cobalt_ebr_register(cobalt_ebr_t *ebr)
{
    (void)ebr;
    return NULL;
}

void cobalt_ebr_unregister(cobalt_ebr_handle_t *handle)
{
    (void)handle;
}

void cobalt_ebr_enter(cobalt_ebr_handle_t *handle)
{
    (void)handle;
}

void cobalt_ebr_exit(cobalt_ebr_handle_t *handle)
{
    (void)handle;
}

void cobalt_ebr_retire(cobalt_ebr_handle_t *handle,
                       void                *ptr,
                       void (*free_fn)(void *ptr, void *user_data),
                       void *user_data)
{
    (void)handle;
    (void)ptr;
    (void)free_fn;
    (void)user_data;
}

bool cobalt_ebr_try_advance(cobalt_ebr_t *ebr)
{
    (void)ebr;
    return false;
}

void cobalt_ebr_sync(cobalt_ebr_t *ebr)
{
    (void)ebr;
}
