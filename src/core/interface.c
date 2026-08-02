/**
 * @file interface.c
 * @brief Implementation of the interface system
 * @details Implements the interface creation, destruction, and implementation checking defined in
 * interface.h.
 */
#include "cobalt/core/interface.h"
#include <stdlib.h>

/**
 * @brief Create and initialize a new interface instance
 *
 * @param vtable Pointer to the interface's virtual function table
 * @return cobalt_interface_t* Newly allocated interface instance, returns NULL if out of memory
 */
cobalt_interface_t *cobalt_interface_new(cobalt_interface_vtable_t *vtable)
{
    /* Allocate memory space for the interface */
    cobalt_interface_t *iface = malloc(sizeof(cobalt_interface_t));
    if (iface) {
        /* Set the virtual function table of the interface */
        iface->vtable = vtable;
    }
    return iface;
}

/**
 * @brief Destroy the interface instance and free memory
 *
 * @param iface Pointer to the interface instance to be destroyed
 */
void cobalt_interface_destroy(cobalt_interface_t *iface)
{
    if (iface) {
        free(iface);
    }
}

/**
 * @brief Check whether the specified object implements a specific interface
 * @details Currently this is a stub implementation, it only checks for null pointers and has not
 * yet implemented specific query logic.
 *
 * @param obj Object pointer to check
 * @param iface Target interface pointer
 * @return int Returns 1 on successful implementation, 0 otherwise
 */
int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface)
{
    /* Check the validity of the input parameters */
    if (!obj || !iface) {
        return 0;
    }
    return 0; /* Placeholder */
}
