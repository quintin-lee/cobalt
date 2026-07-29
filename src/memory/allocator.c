#include "memory/allocator.h"
#include <stdlib.h>

static cobalt_allocator_t system_allocator = {
  .alloc = malloc,
  .free = free,
  .realloc = realloc
};

cobalt_allocator_t *cobalt_allocator_get_system(void) {
  return &system_allocator;
}

void *cobalt_allocator_alloc(cobalt_allocator_t *self, size_t size) {
  return self->alloc(self, size);
}

void cobalt_allocator_free(cobalt_allocator_t *self, void *ptr) {
  self->free(self, ptr);
}

void *cobalt_allocator_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size) {
  return self->realloc(self, ptr, new_size);
}
