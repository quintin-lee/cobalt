/**
 * @file class.c
 * @brief Implementation of the object-oriented class system
 * @details Implements the runtime class creation, method addition, property addition, and class destruction functions defined in class.h.
 */
#include "cobalt/core/class.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a new runtime class
 * 
 * @param name Name of the class
 * @param base_class Base class pointer, can be NULL
 * @return cobalt_class_t* Created class object pointer, returns NULL on failure
 */
cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class)
{
    /* Allocate memory for the class structure */
    cobalt_class_t *cls = malloc(sizeof(cobalt_class_t));
    if (!cls) {
        return NULL;
    }

    /* Initialize all fields of the class, copy the class name */
    cls->name           = cobalt_strdup(name);
    cls->method_count   = 0;
    cls->methods        = NULL;
    cls->property_count = 0;
    cls->properties     = NULL;
    cls->base_class     = base_class;
    cls->abstract       = 0;

    return cls;
}

/**
 * @brief Add a new method to the class
 * @details Currently a stub implementation, only performs parameter validation.
 * 
 * @param cls Target class
 * @param name Method name
 * @param invoke Method invocation pointer
 * @return int Returns 0 on success, -1 on failure
 */
int cobalt_class_add_method(cobalt_class_t *cls,
                            const char     *name,
                            void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count))
{
    /* Check the validity of the input parameters */
    if (!cls || !name || !invoke) {
        return -1;
    }
    /* Prevent unused parameter warnings */
    (void)name;
    (void)invoke;
    return 0;
}

/**
 * @brief Add a new property to the class
 * @details Currently a stub implementation, only performs parameter validation.
 * 
 * @param cls Target class
 * @param name Property name
 * @param get Function pointer to get the property value
 * @param set Function pointer to set the property value
 * @return int Returns 0 on success, -1 on failure
 */
int cobalt_class_add_property(cobalt_class_t *cls,
                              const char     *name,
                              void *(*get)(cobalt_object_t *self),
                              void (*set)(cobalt_object_t *self, void *value))
{
    /* Check the validity of the input parameters, note that get and set might be optional, but class and property name must exist */
    if (!cls || !name) {
        return -1;
    }
    /* Prevent unused parameter warnings */
    (void)get;
    (void)set;
    return 0;
}

/**
 * @brief Check whether the class is an abstract class
 * 
 * @param cls Target class
 * @return int Returns non-zero value if it is an abstract class, 0 otherwise (including the case of passing a null pointer)
 */
int cobalt_class_is_abstract(cobalt_class_t *cls)
{
    return cls ? cls->abstract : 0;
}

/**
 * @brief Destroy the class information object and free memory
 * @details Frees the class name string and the class structure itself.
 * 
 * @param cls Target class
 */
void cobalt_class_destroy(cobalt_class_t *cls)
{
    if (cls) {
        /* The name field is allocated by cobalt_strdup, it must be freed */
        free((void *)cls->name);
        free(cls);
    }
}
