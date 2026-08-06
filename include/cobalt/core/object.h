#ifndef OBJECT_H
#define OBJECT_H

/**
 * @file object.h
 * @brief Base object class, supporting Runtime Type Information (RTTI) and reference counting
 * @details Defines the most foundational structure of the Cobalt object-oriented system.
 *
 * @defgroup CoreObject Core object system
 * @{
 */

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
/** @brief Forward declaration of the class structure */
typedef struct cobalt_class cobalt_class_t;
/** @brief Forward declaration of the interface structure */
typedef struct cobalt_interface cobalt_interface_t;

/**
 * @brief Object header structure (the starting part of all objects)
 * @details All object instances in Cobalt must include this structure at the very beginning of
 * their memory layout, to ensure the normal operation of polymorphism and reference counting.
 */
typedef struct cobalt_object {
    _Atomic uint64_t ref_count; /**< Reference count for memory management (thread-safe) */
    cobalt_class_t *class;      /**< Class pointer for RTTI and method dispatch */
} cobalt_object_t;

/**
 * @brief Increase the reference count of an object
 *
 * @param obj Pointer to the object whose reference count needs to be increased. If NULL, no
 * operation is performed.
 */
void cobalt_object_ref(cobalt_object_t *obj);

/**
 * @brief Decrease the reference count of an object, and free the memory when the count reaches 0
 *
 * @param obj Pointer to the object whose reference count needs to be decreased. If NULL, no
 * operation is performed.
 */
void cobalt_object_unref(cobalt_object_t *obj);

/**
 * @brief Create a new object instance
 * @details Internally allocates enough memory (including header size and extra size),
 *          initializes the reference count to 1, and associates its class information.
 *
 * @param cls Pointer to the class information the object belongs to
 * @param extra_size Extra memory size required by the subclass besides the object header
 * @return Returns the new object pointer on success, or NULL on failure
 */
cobalt_object_t *cobalt_object_new(cobalt_class_t *cls, size_t extra_size);

/**
 * @brief Get the class information the object belongs to
 *
 * @param obj Target object pointer
 * @return Pointer to the class information corresponding to the object, or NULL if NULL is passed
 */
cobalt_class_t *cobalt_object_get_class(cobalt_object_t *obj);

/**
 * @brief Get the current reference count of an object
 *
 * @param obj Target object pointer
 * @return The current reference count, or 0 if obj is NULL
 */
uint64_t cobalt_object_get_ref_count(cobalt_object_t *obj);

/** @} */

#endif /* OBJECT_H */
