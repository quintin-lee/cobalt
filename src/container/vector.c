#include "cobalt/container/vector.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    cobalt_sequence_t base; /* Must be first for polymorphism */
    void** items;
    size_t capacity;
    size_t size;
} cobalt_vector_impl_t;

/* Sequence operations for vector */
static size_t vector_size_seq(cobalt_sequence_t* self)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    return vec->size;
}

static int vector_is_empty_seq(cobalt_sequence_t* self)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    return vec->size == 0;
}

static void vector_add_seq(cobalt_sequence_t* self, void* item)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    if (vec->size >= vec->capacity)
        {
            size_t new_cap = (vec->capacity == 0) ? 1 : vec->capacity * 2;
            void** new_items = (void**)realloc(vec->items, new_cap * sizeof(void*));
            if (!new_items)
                {
                    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
                    return;
                }
            vec->items = new_items;
            vec->capacity = new_cap;
        }
    vec->items[vec->size++] = item;
}

static void vector_remove_seq(cobalt_sequence_t* self, void* item)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    if (!vec || !item)
        return;

    for (size_t i = 0; i < vec->size; i++)
    {
        if (vec->items[i] == item)
        {
            memmove(vec->items + i, vec->items + i + 1,
                    (vec->size - i - 1) * sizeof(void*));
            vec->size--;
            return;
        }
    }
}

static cobalt_iterator_t* vector_iterator_seq(cobalt_sequence_t* self)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    return cobalt_iterator_new((cobalt_sequence_t*)vec);
}

cobalt_vector_t* cobalt_vector_create(size_t initial_capacity)
{
    cobalt_vector_impl_t* vec = malloc(sizeof(cobalt_vector_impl_t));
    if (!vec)
        return NULL;

    vec->items = initial_capacity > 0 ? malloc(initial_capacity * sizeof(void*)) : NULL;
    vec->capacity = initial_capacity;
    vec->size = 0;

    /* Initialize sequence interface */
    vec->base.size = vector_size_seq;
    vec->base.is_empty = vector_is_empty_seq;
    vec->base.add = vector_add_seq;
    vec->base.remove = vector_remove_seq;
    vec->base.iterator = vector_iterator_seq;

    return (cobalt_vector_t*)vec;
}

void cobalt_vector_destroy(cobalt_vector_t* vec)
{
    if (vec)
        {
            free(vec->items);
            free(vec);
        }
}

int cobalt_vector_push(cobalt_vector_t* vec, void* item)
{
    if (!vec)
        {
            cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
            return -1;
        }
    vector_add_seq((cobalt_sequence_t*)vec, item);
    return 0;
}

void* cobalt_vector_get(cobalt_vector_t* vec, size_t index)
{
    if (!vec || index >= vec->size)
        return NULL;
    return vec->items[index];
}

int cobalt_vector_set(cobalt_vector_t* vec, size_t index, void* item)
{
    if (!vec || index >= vec->size)
        {
            cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
            return -1;
        }
    vec->items[index] = item;
    return 0;
}

size_t cobalt_vector_size(cobalt_vector_t* vec)
{
    return vec ? vec->size : 0;
}

int cobalt_vector_is_empty(cobalt_vector_t* vec)
{
    return vec && vec->size == 0;
}
