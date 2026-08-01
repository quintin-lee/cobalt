#ifndef SEQUENCE_H
#define SEQUENCE_H

/**
 * @file sequence.h
 * @brief Sequence (ordered collection) interface
 */

#include <stddef.h>

typedef struct cobalt_sequence cobalt_sequence_t;
typedef struct cobalt_iterator cobalt_iterator_t;

/* Sequence interface */
struct cobalt_sequence
{
    size_t (*size)(cobalt_sequence_t* self);
    int (*is_empty)(cobalt_sequence_t* self);
    void (*add)(cobalt_sequence_t* self, void* item);
    void (*remove)(cobalt_sequence_t* self, void* item);
    cobalt_iterator_t* (*iterator)(cobalt_sequence_t* self);
};

/* Create a sequence */
cobalt_sequence_t* cobalt_sequence_create(size_t initial_capacity);

/* Destroy a sequence */
void cobalt_sequence_destroy(cobalt_sequence_t* seq);

#endif /* SEQUENCE_H */
