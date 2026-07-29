#include "cobalt/core/interface.h"
#include <stdlib.h>

cobalt_interface_t *cobalt_interface_new(cobalt_interface_vtable_t *vtable) {
  cobalt_interface_t *iface = malloc(sizeof(cobalt_interface_t));
  if (iface) iface->vtable = vtable;
  return iface;
}

void cobalt_interface_destroy(cobalt_interface_t *iface) {
  if (iface) free(iface);
}

int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface) {
  if (!obj || !iface) return 0;
  return 0; /* Placeholder */
}
