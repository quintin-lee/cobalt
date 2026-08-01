#include "cobalt/core/class.h"
#include <stdlib.h>
#include <string.h>

cobalt_class_t* cobalt_class_create(const char* name, cobalt_class_t* base_class)
{
    cobalt_class_t* cls = malloc(sizeof(cobalt_class_t));
    if (!cls)
        return NULL;

    cls->name = strdup(name);
    cls->method_count = 0;
    cls->methods = NULL;
    cls->property_count = 0;
    cls->properties = NULL;
    cls->base_class = base_class;
    cls->abstract = 0;

    return cls;
}

int cobalt_class_add_method(cobalt_class_t* cls, const char* name,
                            void* (*invoke)(cobalt_object_t* self, void** args, size_t arg_count))
{
    if (!cls || !name || !invoke)
        return -1;
    (void)name;
    (void)invoke;
    return 0;
}

int cobalt_class_add_property(cobalt_class_t* cls, const char* name,
                              void* (*get)(cobalt_object_t* self),
                              void (*set)(cobalt_object_t* self, void* value))
{
    if (!cls || !name)
        return -1;
    (void)get;
    (void)set;
    return 0;
}

int cobalt_class_is_abstract(cobalt_class_t* cls)
{
    return cls ? cls->abstract : 0;
}

void cobalt_class_destroy(cobalt_class_t* cls)
{
    if (cls)
        {
            free((void*)cls->name);
            free(cls);
        }
}
