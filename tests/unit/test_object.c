/**
 * @file test_object.c
 * @Unit test for core object system (reference counting).
 */

#include "cobalt/core/class.h"
#include "cobalt/core/object.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>

void test_object_lifecycle(void)
{
    printf("Testing object lifecycle...\n");

    /* Create a simple class */
    cobalt_class_t *cls = cobalt_class_create("TestObject", NULL);
    TEST_ASSERT(cls != NULL);

    /* Create an object */
    cobalt_object_t *obj = cobalt_object_new(cls, 0);
    TEST_ASSERT(obj != NULL);
    printf("  Object created\n");

    /* Check initial ref count is 1 */
    uint64_t ref_count = cobalt_object_get_ref_count(obj);
    TEST_EQUAL(ref_count, (uint64_t)1);
    printf("  Initial ref count is 1: OK\n");

    /* Check class pointer */
    cobalt_class_t *obj_class = cobalt_object_get_class(obj);
    TEST_ASSERT(obj_class == cls);
    printf("  Class reference correct: OK\n");

    /* Ref and unref */
    cobalt_object_ref(obj);
    ref_count = cobalt_object_get_ref_count(obj);
    TEST_EQUAL(ref_count, (uint64_t)2);
    printf("  After ref: count=%lu: OK\n", (unsigned long)ref_count);

    cobalt_object_unref(obj);
    ref_count = cobalt_object_get_ref_count(obj);
    TEST_EQUAL(ref_count, (uint64_t)1);
    printf("  After unref: count=%lu: OK\n", (unsigned long)ref_count);

    cobalt_object_unref(obj); /* Final free */
    printf("  Object freed after final unref\n");

    cobalt_class_destroy(cls);
}

void test_object_null_safe(void)
{
    printf("Testing null safety...\n");

    /* These should handle NULL gracefully (no crash, no crash-induced UAF) */
    cobalt_object_ref(NULL);
    cobalt_object_unref(NULL);
    cobalt_class_t *cls = cobalt_object_get_class(NULL);
    TEST_ASSERT(cls == NULL);
    printf("  get_class(NULL) returns NULL: OK\n");

    TEST_EQUAL(cobalt_object_get_ref_count(NULL), (uint64_t)0);
    printf("  get_ref_count(NULL) returns 0: OK\n");

    printf("  Null object handling: OK\n");
}

void test_object(void)
{
    printf("Testing object...\n");
    test_object_lifecycle();
    test_object_null_safe();
    printf("  Object tests completed\n");
}
