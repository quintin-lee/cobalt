#ifndef DEQUE_H
#define DEQUE_H

#include <stddef.h>

typedef struct cobalt_deque cobalt_deque_t;

cobalt_deque_t *cobalt_deque_create(void);
void            cobalt_deque_destroy(cobalt_deque_t *deque);
int             cobalt_deque_push_front(cobalt_deque_t *deque, void *item);
int             cobalt_deque_push_back(cobalt_deque_t *deque, void *item);
void           *cobalt_deque_pop_front(cobalt_deque_t *deque);
void           *cobalt_deque_pop_back(cobalt_deque_t *deque);
void           *cobalt_deque_peek_front(cobalt_deque_t *deque);
void           *cobalt_deque_peek_back(cobalt_deque_t *deque);
size_t          cobalt_deque_size(cobalt_deque_t *deque);
int             cobalt_deque_is_empty(cobalt_deque_t *deque);

#endif /* DEQUE_H */
