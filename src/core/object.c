/**
 * @file object.c
 * @brief Implementation of the base object class
 * @details Implements the object reference counting mechanism and instance allocation logic defined in object.h.
 */
#include "cobalt/core/object.h"
#include <stdlib.h>

/**
 * @brief Increase the reference count of an object
 * 
 * @param obj Pointer to the object whose reference count needs to be increased
 */
void cobalt_object_ref(cobalt_object_t *obj)
{
    if (obj) {
        obj->ref_count++;
    }
}

/**
 * @brief Decrease the reference count of an object
 * @details When the object's reference count decreases to 0, call free() to release the object's memory space.
 * 
 * @param obj Pointer to the object whose reference count needs to be decreased
 */
void cobalt_object_unref(cobalt_object_t *obj)
{
    /* Prefix decrement, if the decremented value is 0, free the object */
    if (obj && --obj->ref_count == 0) {
        free(obj);
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
    size_t           total_size = sizeof(cobalt_object_t) + extra_size;
    cobalt_object_t *obj        = malloc(total_size);
    if (obj) {
        /* Initialize reference count to 1 */
        obj->ref_count = 1;
        /* Associate the corresponding class information (RTTI) */
        obj->class     = cls;
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
