#include "container/treemap.h"
#include <stdlib.h>

static size_t treemap_get(cobalt_map_t *self, const void *key) {
  (void)self; (void)key;
  return 0;
}

static int treemap_put(cobalt_map_t *self, const void *key, void *value) {
  (void)self; (void)key; (void)value;
  return 0;
}

static int treemap_remove(cobalt_map_t *self, const void *key) {
  (void)self; (void)key;
  return 0;
}

static size_t treemap_size(cobalt_map_t *self) {
  cobalt_treemap_t *map = (cobalt_treemap_t *)self;
  return map->size;
}

static int treemap_is_empty(cobalt_map_t *self) {
  return treemap_size(self) == 0;
}

static const cobalt_map_ops treemap_ops = {
  .get = treemap_get,
  .put = treemap_put,
  .remove = treemap_remove,
  .size = treemap_size,
  .is_empty = treemap_is_empty
};

cobalt_treemap_t *cobalt_treemap_create(void) {
  return NULL; /* Simplified */
}

void cobalt_treemap_destroy(cobalt_treemap_t *map) {
  (void)map;
}

int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value) {
  (void)map; (void)key; (void)value;
  return 0;
}

void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key) {
  (void)map; (void)key;
  return NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key) {
  (void)map; (void)key;
  return 0;
}

const char *cobalt_treemap_min_key(cobalt_treemap_t *map) {
  (void)map;
  return NULL;
}

const char *cobalt_treemap_max_key(cobalt_treemap_t *map) {
  (void)map;
  return NULL;
}

size_t cobalt_treemap_size(cobalt_treemap_t *map) {
  return 0;
}
