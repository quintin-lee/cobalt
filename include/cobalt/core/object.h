#ifndef OBJECT_H
#define OBJECT_H

/**
 * @file object.h
 * @brief Base object class with RTTI and reference counting
 */

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct cobalt_class cobalt_class_t;
typedef struct cobalt_interface cobalt_interface_t;

/* Object header (all objects begin with this) */
typedef struct cobalt_object {
  uint64_t ref_count;          /* Reference count for memory management */
  cobalt_class_t *class;       /* Class pointer */
} cobalt_object_t;

/* Object operations */
void cobalt_object_ref(cobalt_object_t *obj);
void cobalt_object_unref(cobalt_object_t *obj);
cobalt_object_t *cobalt_object_new(cobalt_class_t *cls, size_t extra_size);
cobalt_class_t *cobalt_object_get_class(cobalt_object_t *obj);

#endif /* OBJECT_H */
