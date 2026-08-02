#ifndef LIST_H
#define LIST_H

/**
 * @file list.h
 * @brief Doubly-linked list container
 */

#include "cobalt/interface/sequence.h"

typedef struct cobalt_list_node cobalt_list_node_t;
typedef struct cobalt_list      cobalt_list_t;

struct cobalt_list_node {
    void               *data;
    cobalt_list_node_t *next;
    cobalt_list_node_t *prev;
};

struct cobalt_list {
    cobalt_sequence_t   base;
    cobalt_list_node_t *head;
    cobalt_list_node_t *tail;
    size_t              size;
};

/* Create a new list */
cobalt_list_t *cobalt_list_create(void);

/* Destroy the list */
void cobalt_list_destroy(cobalt_list_t *list);

/* Add to beginning */
int cobalt_list_push_front(cobalt_list_t *list, void *item);

/* Add to end */
int cobalt_list_push_back(cobalt_list_t *list, void *item);

/* Remove from beginning */
void *cobalt_list_pop_front(cobalt_list_t *list);

/* Remove from end */
void *cobalt_list_pop_back(cobalt_list_t *list);

/* Get element by index */
void *cobalt_list_get(cobalt_list_t *list, size_t index);

/* List size */
size_t cobalt_list_size(cobalt_list_t *list);

/* Is empty? */
int cobalt_list_is_empty(cobalt_list_t *list);

/* List-specific iterator creation */
cobalt_iterator_t *cobalt_list_iterator_create(cobalt_list_t *list);

#endif /* LIST_H */
