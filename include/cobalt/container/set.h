#ifndef SET_H
#define SET_H

#include <stddef.h>

typedef struct cobalt_set cobalt_set_t;

cobalt_set_t *cobalt_set_create(size_t initial_capacity);
void          cobalt_set_destroy(cobalt_set_t *set);
int           cobalt_set_insert(cobalt_set_t *set, void *item);
int           cobalt_set_remove(cobalt_set_t *set, void *item);
int           cobalt_set_contains(cobalt_set_t *set, void *item);
size_t        cobalt_set_size(cobalt_set_t *set);
int           cobalt_set_is_empty(cobalt_set_t *set);

#endif /* SET_H */
