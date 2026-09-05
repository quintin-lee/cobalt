/**
 * @file object.c
 * @brief Implementation of the base object class
 * @details Implements the object reference counting mechanism and instance allocation logic defined
 * in object.h.
 */
#include "cobalt/core/object.h"
#include "cobalt/core/class.h"
#include "cobalt/memory/allocator.h"
#include <stdatomic.h>
#include <stdlib.h>

static cobalt_allocator_t *object_alloc_for(cobalt_class_t *cls)
{
    if (cls && cls->alloc) {
        return cls->alloc;
    }
    return cobalt_allocator_get_system();
}

/**
 * @brief Increase the reference count of an object
 *
 * @param obj Pointer to the object whose reference count needs to be increased
 */
void cobalt_object_ref(cobalt_object_t *obj)
{
    if (obj) {
        atomic_fetch_add_explicit(&obj->ref_count, 1, memory_order_relaxed);
    }
}

/**
 * @brief Decrease the reference count of an object
 * @details When the object's reference count decreases to 0, call free() to release the object's
 * memory space.
 *
 * @param obj Pointer to the object whose reference count needs to be decreased
 */
void cobalt_object_unref(cobalt_object_t *obj)
{
    /* fetch_sub returns the OLD value; we free when it was 1 (going to 0) */
    if (obj && atomic_fetch_sub_explicit(&obj->ref_count, 1, memory_order_acq_rel) == 1) {
        cobalt_allocator_t *alloc = object_alloc_for(obj->class);
        alloc->free(alloc, obj);
    }
}

/**
 * @brief Allocate a new object based on the specified class and extra size
 *
 * @param cls Object's class pointer
 * @param extra_size Extra memory size (space required for subclass data members)
 * @return cobalt_object_t* Newly allocated object pointer
 */
cobalt_object_t *cobalt_object_new(cobalt_class_t *cls, size_t extra_size)
{
    /* Calculate the total required memory size: base object header + extra data space */
    size_t              total_size = sizeof(cobalt_object_t) + extra_size;
    cobalt_allocator_t *alloc      = object_alloc_for(cls);
    cobalt_object_t    *obj        = alloc->alloc(alloc, total_size);
    if (obj) {
        /* Initialize reference count to 1 */
        atomic_store_explicit(&obj->ref_count, 1, memory_order_relaxed);
        /* Associate the corresponding class information (RTTI) */
        obj->class = cls;
    }
    return obj;
}

/**
 * @brief Get the runtime class information of the object
 *
 * @param obj Object pointer
 * @return cobalt_class_t* Corresponding class pointer
 */
cobalt_class_t *cobalt_object_get_class(cobalt_object_t *obj)
{
    return obj ? obj->class : NULL;
}

/**
 * @brief Get the current reference count of an object
 *
 * @param obj Object pointer
 * @return The current reference count, or 0 if obj is NULL
 */
uint64_t cobalt_object_get_ref_count(cobalt_object_t *obj)
{
    return obj ? atomic_load_explicit(&obj->ref_count, memory_order_relaxed) : 0;
}
