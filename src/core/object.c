#include "core/object.h"
#include <stdlib.h>

void cobalt_object_ref(cobalt_object_t *obj) {
  if (obj) obj->ref_count++;
}

void cobalt_object_unref(cobalt_object_t *obj) {
  if (obj && --obj->ref_count == 0) {
    free(obj);
  }
}

cobalt_object_t *cobalt_object_new(cobalt_class_t *cls, size_t extra_size) {
  size_t total_size = sizeof(cobalt_object_t) + extra_size;
  cobalt_object_t *obj = malloc(total_size);
  if (obj) {
    obj->ref_count = 1;
    obj->class = cls;
  }
  return obj;
}

cobalt_class_t *cobalt_object_get_class(cobalt_object_t *obj) {
  return obj ? obj->class : NULL;
}
