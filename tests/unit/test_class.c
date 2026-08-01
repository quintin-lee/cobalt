/**
 * @file test_class.c
 * @Unit test for class metadata system.
 */

#include "cobalt/core/class.h"
#include "cobalt/core/object.h"
#include <stdio.h>
#include <string.h>

/* Dummy method implementations */
static void* dummy_method(cobalt_object_t* self, void** args, size_t arg_count)
{
    (void)self;
    (void)args;
    (void)arg_count;
    return NULL;
}

void test_class_create_destroy(void)
{
    printf("Testing class create/destroy...\n");

    /* Create base class */
    cobalt_class_t* base = cobalt_class_create("BaseClass", NULL);
    if (!base)
        {
            fprintf(stderr, "ERROR: Failed to create base class\n");
            return;
        }
    printf("  Base class created\n");

    /* Create derived class */
    cobalt_class_t* derived = cobalt_class_create("DerivedClass", base);
    if (!derived)
        {
            fprintf(stderr, "ERROR: Failed to create derived class\n");
            cobalt_class_destroy(base);
            return;
        }
    printf("  Derived class created with base\n");

    /* Cleanup */
    cobalt_class_destroy(derived);
    cobalt_class_destroy(base);
    printf("  Classes destroyed\n");
}

void test_class_methods(void)
{
    printf("Testing class methods...\n");

    cobalt_class_t* cls = cobalt_class_create("TestClass", NULL);
    if (!cls)
        {
            fprintf(stderr, "ERROR: Failed to create class\n");
            return;
        }

    /* Add a method */
    int ret = cobalt_class_add_method(cls, "test_method", dummy_method);
    if (ret == 0)
        {
            printf("  Method added successfully\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Failed to add method\n");
        }

    /* Try adding method to NULL class */
    ret = cobalt_class_add_method(NULL, "test", dummy_method);
    if (ret == -1)
        {
            printf("  Add method to NULL class returns -1: OK\n");
        }

    /* Try adding method with NULL name */
    ret = cobalt_class_add_method(cls, NULL, dummy_method);
    if (ret == -1)
        {
            printf("  Add method with NULL name returns -1: OK\n");
        }

    /* Try adding method with NULL invoke */
    ret = cobalt_class_add_method(cls, "test2", NULL);
    if (ret == -1)
        {
            printf("  Add method with NULL invoke returns -1: OK\n");
        }

    cobalt_class_destroy(cls);
}

void test_class_properties(void)
{
    printf("Testing class properties...\n");

    cobalt_class_t* cls = cobalt_class_create("PropertyClass", NULL);
    if (!cls)
        {
            fprintf(stderr, "ERROR: Failed to create class\n");
            return;
        }

    /* Add a property */
    int ret = cobalt_class_add_property(cls, "test_prop", NULL, NULL);
    if (ret == 0)
        {
            printf("  Property added successfully\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Failed to add property\n");
        }

    /* Try adding property to NULL class */
    ret = cobalt_class_add_property(NULL, "test", NULL, NULL);
    if (ret == -1)
        {
            printf("  Add property to NULL class returns -1: OK\n");
        }

    /* Try adding property with NULL name */
    ret = cobalt_class_add_property(cls, NULL, NULL, NULL);
    if (ret == -1)
        {
            printf("  Add property with NULL name returns -1: OK\n");
        }

    cobalt_class_destroy(cls);
}

void test_class_abstract(void)
{
    printf("Testing abstract class flag...\n");

    cobalt_class_t* cls = cobalt_class_create("AbstractClass", NULL);
    if (!cls)
        {
            fprintf(stderr, "ERROR: Failed to create class\n");
            return;
        }

    /* Check abstract flag (should be 0 by default) */
    int is_abstract = cobalt_class_is_abstract(cls);
    if (is_abstract == 0)
        {
            printf("  Default class is not abstract: OK\n");
        }

    /* Check NULL class */
    is_abstract = cobalt_class_is_abstract(NULL);
    if (is_abstract == 0)
        {
            printf("  NULL class returns 0 for is_abstract: OK\n");
        }

    cobalt_class_destroy(cls);
}

void test_class_inheritance(void)
{
    printf("Testing class inheritance...\n");

    /* Create base class */
    cobalt_class_t* base = cobalt_class_create("Base", NULL);
    if (!base)
        {
            fprintf(stderr, "ERROR: Failed to create base class\n");
            return;
        }

    /* Create derived class */
    cobalt_class_t* derived = cobalt_class_create("Derived", base);
    if (!derived)
        {
            fprintf(stderr, "ERROR: Failed to create derived class\n");
            cobalt_class_destroy(base);
            return;
        }

    /* Create further derived class */
    cobalt_class_t* derived2 = cobalt_class_create("Derived2", derived);
    if (derived2)
        {
            printf("  Multi-level inheritance works: OK\n");
            cobalt_class_destroy(derived2);
        }

    cobalt_class_destroy(derived);
    cobalt_class_destroy(base);
}

void test_class(void)
{
    printf("Testing class...\n");
    test_class_create_destroy();
    test_class_methods();
    test_class_properties();
    test_class_abstract();
    test_class_inheritance();
    printf("  Class tests completed\n");
}
