#ifndef ITERATOR_H
#define ITERATOR_H

/**
 * @file iterator.h
 * @brief Iterator for traversing sequences
 */

#include "sequence.h"
#include <stddef.h>

typedef struct cobalt_iterator cobalt_iterator_t;

/* Iterator operations */
cobalt_iterator_t* cobalt_iterator_new(cobalt_sequence_t* seq);
int cobalt_iterator_has_next(cobalt_iterator_t* iter);
void* cobalt_iterator_next(cobalt_iterator_t* iter);
void cobalt_iterator_destroy(cobalt_iterator_t* iter);

#endif /* ITERATOR_H */
