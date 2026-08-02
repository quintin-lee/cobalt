#ifndef ITERATOR_H
#define ITERATOR_H

#include "sequence.h"
#include <stddef.h>

/* Iterator vtable for polymorphism */
typedef struct cobalt_iterator_vtable
{
    int (*has_next)(void* iter);
    void* (*next)(void* iter);
    void (*destroy)(void* iter);
} cobalt_iterator_vtable_t;

typedef struct cobalt_iterator cobalt_iterator_t;

struct cobalt_iterator
{
    const cobalt_iterator_vtable_t* vtable;
    void* data;
};

/* Iterator operations */
cobalt_iterator_t* cobalt_iterator_new(cobalt_sequence_t* seq);
int cobalt_iterator_has_next(cobalt_iterator_t* iter);
void* cobalt_iterator_next(cobalt_iterator_t* iter);
void cobalt_iterator_destroy(cobalt_iterator_t* iter);

#endif /* ITERATOR_H */
