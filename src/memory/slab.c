#include <stdint.h>
/**
 * @file slab.c
 * @brief Object-caching slab allocator implementation
 */

#include "cobalt/memory/slab.h"
#include <stdlib.h>
#include <string.h>

#define COBALT_SLAB_MAX_CLASSES 16

typedef struct {
    size_t block_size;
    size_t block_count;
    size_t free_count;
    void  *memory;
    void  *free_list;
} cobalt_slab_class_t;

struct cobalt_slab {
    cobalt_slab_class_t classes[COBALT_SLAB_MAX_CLASSES];
    size_t              class_count;
};

static int slab_find_class(cobalt_slab_t *slab, size_t size)
{
    for (size_t i = 0; i < slab->class_count; i++) {
        if (size <= slab->classes[i].block_size) {
            return (int)i;
        }
    }
    return -1;
}

cobalt_slab_t *cobalt_slab_create(const size_t *sizes, const size_t *counts, size_t class_count)
{
    if (!sizes || !counts || class_count == 0 || class_count > COBALT_SLAB_MAX_CLASSES) {
        return NULL;
    }

    cobalt_slab_t *slab = (cobalt_slab_t *)calloc(1, sizeof(cobalt_slab_t));
    if (!slab) {
        return NULL;
    }

    for (size_t i = 0; i < class_count; i++) {
        size_t bs = sizes[i];
        if (bs < sizeof(void *)) {
            bs = sizeof(void *);
        }
        size_t cnt = counts[i];
        if (cnt == 0) {
            cnt = 16;
        }

        cobalt_slab_class_t *cls = &slab->classes[i];
        cls->block_size          = bs;
        cls->block_count         = cnt;
        cls->free_count          = cnt;
        cls->memory              = calloc(1, bs * cnt);
        if (!cls->memory) {
            cobalt_slab_destroy(slab);
            return NULL;
        }

        /* Build free-list */
        cls->free_list      = cls->memory;
        unsigned char *base = (unsigned char *)cls->memory;
        for (size_t j = 0; j < cnt - 1; j++) {
            void **next = (void **)(base + j * bs);
            *next       = (void *)(base + (j + 1) * bs);
        }
        void **last = (void **)(base + (cnt - 1) * bs);
        *last       = NULL;
    }

    slab->class_count = class_count;
    return slab;
}

void cobalt_slab_destroy(cobalt_slab_t *slab)
{
    if (!slab) {
        return;
    }
    for (size_t i = 0; i < slab->class_count; i++) {
        free(slab->classes[i].memory);
    }
    free(slab);
}

void *cobalt_slab_alloc(cobalt_slab_t *slab, size_t size)
{
    if (!slab || size == 0) {
        return NULL;
    }

    int idx = slab_find_class(slab, size);
    if (idx < 0) {
        return NULL;
    }

    cobalt_slab_class_t *cls = &slab->classes[(size_t)idx];
    if (cls->free_count == 0) {
        return NULL;
    }

    void *block    = cls->free_list;
    cls->free_list = *(void **)block;
    cls->free_count--;
    memset(block, 0, cls->block_size);
    return block;
}

void cobalt_slab_free(cobalt_slab_t *slab, void *ptr)
{
    if (!slab || !ptr) {
        return;
    }

    for (size_t i = 0; i < slab->class_count; i++) {
        cobalt_slab_class_t *cls    = &slab->classes[i];
        unsigned char       *base   = (unsigned char *)cls->memory;
        uintptr_t            offset = (uintptr_t)ptr - (uintptr_t)base;
        if (offset >= cls->block_size * cls->block_count) {
            continue;
        }
        if (offset % cls->block_size != 0) {
            continue;
        }
        *(void **)ptr  = cls->free_list;
        cls->free_list = ptr;
        cls->free_count++;
        return;
    }
}
