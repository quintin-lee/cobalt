#ifndef INTERFACE_H
#define INTERFACE_H

/**
 * @file interface.h
 * @brief Interface (pure virtual base class) system
 * @details Provides the definition and operations of interfaces in the Cobalt framework, supporting
 * polymorphism and multiple interface implementations.
 *
 * @defgroup CoreInterface Core interface system
 * @{
 */

#include "object.h"
#include <stddef.h>

/** @brief Forward declaration of the interface structure */
typedef struct cobalt_interface cobalt_interface_t;

/**
 * @brief Interface virtual function table (vtable)
 * @details Defines the methods that the interface must implement, currently only contains the
 * destructor.
 */
typedef struct cobalt_interface_vtable {
    /**
     * @brief Interface destructor function pointer
     * @param self The interface instance to be destroyed
     */
    void (*destroy)(cobalt_interface_t *self);
} cobalt_interface_vtable_t;

/**
 * @brief Interface instance definition structure
 * @details Each interface instance contains a pointer to its virtual function table to implement
 * polymorphism.
 */
struct cobalt_interface {
    cobalt_interface_vtable_t
        *vtable; /**< Interface's virtual function table pointer (vtable pointer) */
};

/**
 * @name Interface operation functions
 * @{
 */

/**
 * @brief Create a new interface instance
 *
 * @param vtable The virtual function table pointer corresponding to this interface
 * @return Successfully created interface pointer, returns NULL on allocation failure
 */
cobalt_interface_t *cobalt_interface_new(cobalt_interface_vtable_t *vtable);

/**
 * @brief Destroy the specified interface instance
 *
 * @param iface The interface pointer to be destroyed. No operation if NULL.
 */
void cobalt_interface_destroy(cobalt_interface_t *iface);

/**
 * @brief Check whether an object implements the specified interface
 *
 * @param obj The target object pointer to check
 * @param iface The interface pointer to verify
 * @return Returns non-zero if the object implements the interface, 0 otherwise
 */
int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface);

/** @} */

/** @} */

#endif /* INTERFACE_H */
