/**
 * @file class_hierarchy.c
 * @brief Demonstrates object-oriented programming patterns with Cobalt
 *
 * Shows:
 * - Creating base and derived classes
 * - Adding methods to classes
 * - Object creation and reference counting
 * - Interface querying
 */

#include <stdio.h>
#include <stdlib.h>
#include <cobalt/cobalt.h>

/* Forward declare our custom class */
static const cobalt_class_t *BaseClass = NULL;
static const cobalt_class_t *DerivedClass = NULL;

/* Method implementation for BaseClass */
void *base_invoke(cobalt_object_t *self, void **args, size_t arg_count) {
    (void)arg_count;
    (void)args;
    printf("Base method invoked on object: %s\n", ((cobalt_object_t *)self)->class->name);
    return NULL;
}

int main(void) {
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Create a base class */
    BaseClass = cobalt_class_create("BaseClass", NULL);
    if (!BaseClass) {
        fprintf(stderr, "Failed to create BaseClass\n");
        return 1;
    }

    /* Add a method to base class */
    cobalt_class_add_method(BaseClass, "invoke", base_invoke);

    /* Create a derived class inheriting from BaseClass */
    DerivedClass = cobalt_class_create("DerivedClass", BaseClass);
    if (!DerivedClass) {
        fprintf(stderr, "Failed to create DerivedClass\n");
        return 1;
    }

    /* Create an instance of derived class */
    cobalt_object_t *obj = cobalt_object_new(DerivedClass, 0);
    if (!obj) {
        fprintf(stderr, "Failed to create object\n");
        return 1;
    }

    cobalt_info("Object created: class=%s, ref_count=%lu\n",
                cobalt_object_get_class(obj)->name, obj->ref_count);

    /* Use reference counting */
    cobalt_object_ref(obj);
    cobalt_info("After ref, ref_count=%lu\n", obj->ref_count);

    /* Clean up - unref twice will free the object */
    cobalt_object_unref(obj);
    cobalt_object_unref(obj);

    /* Destroy class metadata */
    cobalt_class_destroy(DerivedClass);
    cobalt_class_destroy(BaseClass);

    cobalt_info("Done!\n");
    return 0;
}