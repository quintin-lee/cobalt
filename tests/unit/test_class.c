/**
 * @file test_class.c
 * @Unit test for class metadata system.
 */

#include <stdio.h>
#include <string.h>
#include "cobalt/core/class.h"
#include "cobalt/core/object.h"

/* Dummy method implementations */
static void* dummy_method(cobalt_object_t *self, void **args, size_t arg_count) {
    (void)self; (void)args; (void)arg_count;
    return NULL;
}

void test_class_create_destroy(void) {
    printf("Testing class create/destroy...\n");
    
    /* Create base class */
    cobalt_class_t *base = cobalt_class_create("BaseClass", NULL);
    if (!base) {
        fprintf(stderr, "ERROR: Failed to create base class\n");
        return;
    }
    printf("  Base class created\n");
    
    /* Create derived class */
    cobalt_class_t *derived = cobalt_class_create("DerivedClass", base);
    if (!derived) {
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

void test_class_methods(void) {
    printf("Testing class methods...\n");
    
    cobalt_class_t *cls = cobalt_class_create("TestClass", NULL);
    if (!cls) {
        fprintf(stderr, "ERROR: Failed to create class\n");
        return;
    }
    
    /* Add a method */
    int ret = cobalt_class_add_method(cls, "test_method", dummy_method);
    if (ret == 0) {
        printf("  Method added successfully\n");
    } else {
        fprintf(stderr, "ERROR: Failed to add method\n");
    }
    
    /* Try duplicate (should fail or overwrite - implementation dependent) */
    ret = cobalt_class_add_method(cls, "test_method", dummy_method);
    printf("  Duplicate method attempt: ret=%d\n", ret);
    
    cobalt_class_destroy(cls);
}

void test_class_abstract(void) {
    printf("Testing abstract class flag...\n");
    
    cobalt_class_t *abstract_cls = cobalt_class_create("AbstractBase", NULL);
    /* Note: set abstract flag through internal mechanism would be needed */
    /* For now just verify creation works */
    if (abstract_cls) {
        printf("  Abstract class created\n");
        cobalt_class_destroy(abstract_cls);
    }
}

void test_class(void) {
    printf("Testing class...\n");
    test_class_create_destroy();
    test_class_methods();
    test_class_abstract();
    printf("  Class tests completed\n");
}
