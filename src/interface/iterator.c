#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"
#include <stdlib.h>

/* Generic iterator implementation for sequences */
typedef struct
{
    cobalt_sequence_t* seq;
    size_t index;
    size_t total;
} cobalt_iterator_impl_t;

/* Vtable for generic iterator */
static int generic_has_next(void* ctx)
{
    cobalt_iterator_impl_t* impl = (cobalt_iterator_impl_t*)ctx;
    return impl->index < impl->total;
}

static void* generic_next(void* ctx)
{
    cobalt_iterator_impl_t* impl = (cobalt_iterator_impl_t*)ctx;
    if (impl->index >= impl->total)
        return NULL;
    impl->index++;
    return NULL; /* Generic iterator doesn't support element access */
}

static void generic_destroy(void* ctx)
{
    if (ctx)
        free(ctx);
}

static const cobalt_iterator_vtable_t generic_vtable = {
    .has_next = generic_has_next,
    .next = generic_next,
    .destroy = generic_destroy
};

cobalt_iterator_t* cobalt_iterator_new(cobalt_sequence_t* seq)
{
    if (!seq)
        return NULL;

    cobalt_iterator_t* iter = malloc(sizeof(cobalt_iterator_t));
    if (!iter)
        return NULL;

    cobalt_iterator_impl_t* impl = malloc(sizeof(cobalt_iterator_impl_t));
    if (!impl)
        {
            free(iter);
            return NULL;
        }

    impl->seq = seq;
    impl->index = 0;
    impl->total = seq->size(seq);

    iter->vtable = &generic_vtable;
    iter->data = impl;
    return iter;
}

int cobalt_iterator_has_next(cobalt_iterator_t* iter)
{
    if (!iter || !iter->vtable)
        return 0;
    return iter->vtable->has_next(iter->data);
}

void* cobalt_iterator_next(cobalt_iterator_t* iter)
{
    if (!iter || !iter->vtable)
        return NULL;
    return iter->vtable->next(iter->data);
}

void cobalt_iterator_destroy(cobalt_iterator_t* iter)
{
    if (!iter)
        return;
    if (iter->vtable && iter->vtable->destroy)
        iter->vtable->destroy(iter->data);
    free(iter);
}
