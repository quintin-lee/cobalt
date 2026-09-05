/**
 * @file class.c
 * @brief Implementation of the object-oriented class system
 * @details Implements the runtime class creation, method addition, property addition, and class
 * destruction functions defined in class.h.
 */
#include "cobalt/core/class.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

static char *class_strdup(cobalt_allocator_t *alloc, const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char  *dup = alloc->alloc(alloc, len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

/**
 * @brief Create a new runtime class
 *
 * @param name Name of the class
 * @param base_class Base class pointer, can be NULL
 * @return cobalt_class_t* Created class object pointer, returns NULL on failure
 */
cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class)
{
    return cobalt_class_create_with_allocator(name, base_class, cobalt_allocator_get_system());
}

/**
 * @brief Create a new runtime class with a custom allocator
 *
 * @param name Name of the class
 * @param base_class Base class pointer, can be NULL
 * @param alloc Custom allocator, or NULL to fall back to the system allocator
 * @return cobalt_class_t* Created class object pointer, returns NULL on failure
 */
cobalt_class_t *cobalt_class_create_with_allocator(const char         *name,
                                                   cobalt_class_t     *base_class,
                                                   cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    /* Allocate memory for the class structure */
    cobalt_class_t *cls = alloc->alloc(alloc, sizeof(cobalt_class_t));
    if (!cls) {
        return NULL;
    }

    /* Initialize all fields of the class, copy the class name */
    cls->name           = class_strdup(alloc, name);
    cls->method_count   = 0;
    cls->methods        = NULL;
    cls->property_count = 0;
    cls->properties     = NULL;
    cls->base_class     = base_class;
    cls->abstract       = 0;
    cls->alloc          = alloc;

    return cls;
}

/**
 * @brief Add a new method to the class
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
    if (!cls || !name || !invoke) {
        return -1;
    }

    cobalt_allocator_t *alloc = cls->alloc;
    cobalt_method_t   **new_methods =
        alloc->realloc(alloc, cls->methods, sizeof(cobalt_method_t *) * (cls->method_count + 1));
    if (!new_methods) {
        return -1;
    }
    cls->methods = new_methods;

    cls->methods[cls->method_count] = alloc->alloc(alloc, sizeof(cobalt_method_t));
    if (!cls->methods[cls->method_count]) {
        return -1;
    }
    cls->methods[cls->method_count]->name   = class_strdup(alloc, name);
    cls->methods[cls->method_count]->invoke = invoke;
    cls->method_count++;

    return 0;
}

/**
 * @brief Add a new property to the class
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
    if (!cls || !name) {
        return -1;
    }

    cobalt_allocator_t *alloc          = cls->alloc;
    cobalt_property_t **new_properties = alloc->realloc(
        alloc, cls->properties, sizeof(cobalt_property_t *) * (cls->property_count + 1));
    if (!new_properties) {
        return -1;
    }
    cls->properties = new_properties;

    cls->properties[cls->property_count] = alloc->alloc(alloc, sizeof(cobalt_property_t));
    if (!cls->properties[cls->property_count]) {
        return -1;
    }
    cls->properties[cls->property_count]->name = class_strdup(alloc, name);
    cls->properties[cls->property_count]->get  = get;
    cls->properties[cls->property_count]->set  = set;
    cls->property_count++;

    return 0;
}

/**
 * @brief Check whether the class is an abstract class
 *
 * @param cls Target class
 * @return int Returns non-zero value if it is an abstract class, 0 otherwise (including the case of
 * passing a null pointer)
 */
int cobalt_class_is_abstract(cobalt_class_t *cls)
{
    return cls ? cls->abstract : 0;
}

/**
 * @brief Destroy the class information object and free memory
 * @details Frees the class name string, method table, property table, and the class structure
 * itself.
 *
 * @param cls Target class
 */
void cobalt_class_destroy(cobalt_class_t *cls)
{
    if (cls) {
        cobalt_allocator_t *alloc = cls->alloc;
        /* Free method names and method structures */
        if (cls->methods) {
            for (size_t i = 0; i < cls->method_count; i++) {
                alloc->free(alloc, (void *)cls->methods[i]->name);
                alloc->free(alloc, cls->methods[i]);
            }
            alloc->free(alloc, cls->methods);
        }

        /* Free property names and property structures */
        if (cls->properties) {
            for (size_t i = 0; i < cls->property_count; i++) {
                alloc->free(alloc, (void *)cls->properties[i]->name);
                alloc->free(alloc, cls->properties[i]);
            }
            alloc->free(alloc, cls->properties);
        }

        alloc->free(alloc, (void *)cls->name);
        alloc->free(alloc, cls);
    }
}
