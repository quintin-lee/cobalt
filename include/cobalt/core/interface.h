#ifndef INTERFACE_H
#define INTERFACE_H

/**
 * @file interface.h
 * @brief Interface (pure abstract base) system
 */

#include <stddef.h>
#include "object.h"

typedef struct cobalt_interface cobalt_interface_t;

/* Interface vtable */
typedef struct cobalt_interface_vtable {
  void (*destroy)(cobalt_interface_t *self);
} cobalt_interface_vtable_t;

/* Interface definition */
struct cobalt_interface {
  cobalt_interface_vtable_t *vtable;
};

/* Interface operations */
cobalt_interface_t *cobalt_interface_new(cobalt_interface_vtable_t *vtable);
void cobalt_interface_destroy(cobalt_interface_t *iface);

/* Check if object implements an interface */
int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface);

#endif /* INTERFACE_H */
