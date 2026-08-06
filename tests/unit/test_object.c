/**
 * @file test_object.c
 * @Unit test for core object system (reference counting).
 */

#include "cobalt/core/class.h"
#include "cobalt/core/object.h"
#include <stdio.h>
#include <stdlib.h>

void test_object_lifecycle(void)
{
    printf("Testing object lifecycle...\n");

    /* Create a simple class */
    cobalt_class_t *cls = cobalt_class_create("TestObject", NULL);
    if (!cls) {
        fprintf(stderr, "ERROR: Failed to create class\n");
        return;
    }

    /* Create an object */
    cobalt_object_t *obj = cobalt_object_new(cls, 0);
    if (!obj) {
        fprintf(stderr, "ERROR: Failed to create object\n");
        cobalt_class_destroy(cls);
        return;
    }
    printf("  Object created\n");

    /* Check initial ref count is 1 */
    uint64_t ref_count = cobalt_object_get_ref_count(obj);
    if (ref_count == 1) {
        printf("  Initial ref count is 1: OK\n");
    } else {
        fprintf(stderr, "ERROR: Expected ref count 1, got %lu\n", (unsigned long)ref_count);
    }

    /* Check class pointer */
    cobalt_class_t *obj_class = cobalt_object_get_class(obj);
    if (obj_class == cls) {
        printf("  Class reference correct: OK\n");
    } else {
        fprintf(stderr, "ERROR: Class mismatch\n");
    }

    /* Ref and unref */
    cobalt_object_ref(obj);
    ref_count = cobalt_object_get_ref_count(obj);
    if (ref_count == 2) {
        printf("  After ref: count=%lu: OK\n", (unsigned long)ref_count);
    }

    cobalt_object_unref(obj);
    ref_count = cobalt_object_get_ref_count(obj);
    if (ref_count == 1) {
        printf("  After unref: count=%lu: OK\n", (unsigned long)ref_count);
    }

    cobalt_object_unref(obj); /* Final free */
    printf("  Object freed after final unref\n");

    cobalt_class_destroy(cls);
}

void test_object_null_safe(void)
{
    printf("Testing null safety...\n");

    /* These should handle NULL gracefully */
    cobalt_object_ref(NULL);
    cobalt_object_unref(NULL);
    cobalt_class_t *cls = cobalt_object_get_class(NULL);
    if (cls == NULL) {
        printf("  Null object handling: OK\n");
    }
}

void test_object(void)
{
    printf("Testing object...\n");
    test_object_lifecycle();
    test_object_null_safe();
    printf("  Object tests completed\n");
}
