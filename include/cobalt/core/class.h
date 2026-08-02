#ifndef CLASS_H
#define CLASS_H

/**
 * @file class.h
 * @brief Object-oriented class system
 * @details Provides the definition of the runtime class structure, supporting the construction of
 * class information, registration of methods and properties, and inheritance mechanisms.
 *
 * @defgroup CoreClass Core class system
 * @{
 */

#include "object.h"
#include <stddef.h>

/** @brief Forward declaration of the method structure */
typedef struct cobalt_method cobalt_method_t;
/** @brief Forward declaration of the property structure */
typedef struct cobalt_property cobalt_property_t;

/**
 * @brief Runtime class definition structure
 * @details Stores class metadata, including name, method table, property table, and inheritance
 * relationships.
 */
typedef struct cobalt_class {
    const char          *name;           /**< Class name */
    size_t               method_count;   /**< Number of methods registered */
    cobalt_method_t    **methods;        /**< Method table array pointer */
    size_t               property_count; /**< Number of properties registered */
    cobalt_property_t  **properties;     /**< Property table array pointer */
    struct cobalt_class *base_class;     /**< Pointer to the base class, used for inheritance */
    int                  abstract;       /**< Flag indicating if it is an abstract class */
} cobalt_class_t;

/**
 * @brief Method definition structure
 * @details Describes a method of a class, including the method name and the corresponding function
 * pointer.
 */
struct cobalt_method {
    const char *name; /**< Method name */
    /**
     * @brief Method invocation function pointer
     * @param self Target object of the method invocation (this pointer)
     * @param args Array of arguments passed to the method
     * @param arg_count Number of arguments
     * @return Pointer to the method's return value
     */
    void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count);
};

/**
 * @brief Property definition structure
 * @details Describes a property of a class, including the property name and its getter and setter.
 */
struct cobalt_property {
    const char *name; /**< Property name */
    /**
     * @brief Property getter function pointer
     * @param self Target object
     * @return Pointer to the returned property value
     */
    void *(*get)(cobalt_object_t *self);
    /**
     * @brief Property setter function pointer
     * @param self Target object
     * @param value Pointer to the new property value
     */
    void (*set)(cobalt_object_t *self, void *value);
};

/**
 * @name Class operation functions
 * @{
 */

/**
 * @brief Create a new class
 *
 * @param name Name of the class
 * @param base_class Optional base class pointer. If NULL, there is no base class.
 * @return Newly created class pointer. Returns NULL on failure.
 */
cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class);

/**
 * @brief Add a new method to the specified class
 *
 * @param cls Target class pointer
 * @param name Method name
 * @param invoke The actual execution function of the method
 * @return Returns 0 on success, non-zero value on failure
 */
int cobalt_class_add_method(cobalt_class_t *cls,
                            const char     *name,
                            void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count));

/**
 * @brief Add a new property to the specified class
 *
 * @param cls Target class pointer
 * @param name Property name
 * @param get Property getter function
 * @param set Property setter function
 * @return Returns 0 on success, non-zero value on failure
 */
int cobalt_class_add_property(cobalt_class_t *cls,
                              const char     *name,
                              void *(*get)(cobalt_object_t *self),
                              void (*set)(cobalt_object_t *self, void *value));

/**
 * @brief Check if a class is an abstract class
 *
 * @param cls Target class pointer
 * @return Returns non-zero value if it is an abstract class, 0 otherwise
 */
int cobalt_class_is_abstract(cobalt_class_t *cls);

/**
 * @brief Destroy the class and free related memory
 *
 * @param cls Pointer to the class to be destroyed
 */
void cobalt_class_destroy(cobalt_class_t *cls);

/** @} */

/** @} */

#endif /* CLASS_H */
