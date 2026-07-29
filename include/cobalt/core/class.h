#ifndef CLASS_H
#define CLASS_H

/**
 * @file class.h
 * @brief Object-oriented class system
 */

#include <stddef.h>
#include "object.h"

typedef struct cobalt_method cobalt_method_t;
typedef struct cobalt_property cobalt_property_t;

/* Class definition */
typedef struct cobalt_class {
  const char *name;                   /* Class name */
  size_t method_count;                /* Number of methods */
  cobalt_method_t **methods;          /* Method table */
  size_t property_count;              /* Number of properties */
  cobalt_property_t **properties;     /* Property table */
  cobalt_class_t *base_class;         /* Base class (inheritance) */
  int abstract;                       /* Is abstract class? */
} cobalt_class_t;

/* Method definition */
struct cobalt_method {
  const char *name;
  void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count);
};

/* Property definition */
struct cobalt_property {
  const char *name;
  void *(*get)(cobalt_object_t *self);
  void (*set)(cobalt_object_t *self, void *value);
};

/* Class operations */
cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class);
int cobalt_class_add_method(cobalt_class_t *cls, const char *name, void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count));
int cobalt_class_add_property(cobalt_class_t *cls, const char *name, void *(*get)(cobalt_object_t *self), void (*set)(cobalt_object_t *self, void *value));
int cobalt_class_is_abstract(cobalt_class_t *cls);
void cobalt_class_destroy(cobalt_class_t *cls);

#endif /* CLASS_H */
