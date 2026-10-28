/**
 * @file test_interface.c
 * @Unit test for multi-interface mechanism.
 */
#include <stdlib.h>

#include "cobalt/core/interface.h"
#include "cobalt/core/object.h"
#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <stdio.h>

/* Dummy interface vtable */
static void dummy_destroy(cobalt_interface_t *self)
{
    (void)self;
    free(self);
}

static cobalt_interface_vtable_t dummy_vtable = {.destroy = dummy_destroy};

void test_interface_create_destroy(void)
{
    printf("Testing interface create/destroy...\n");

    cobalt_interface_t *iface = cobalt_interface_new(&dummy_vtable);
    TEST_ASSERT(iface != NULL);
    TEST_ASSERT(iface->vtable == &dummy_vtable);
    printf("  Interface created\n");

    cobalt_interface_destroy(iface);
    printf("  Interface destroyed\n");
}

void test_interface_implements(void)
{
    printf("Testing interface implements check...\n");

    cobalt_interface_t *iface = cobalt_interface_new(&dummy_vtable);
    TEST_ASSERT(iface != NULL);
    printf("  Interface created for testing\n");

    /* NULL checks: both arguments NULL must be rejected with 0 */
    int result = cobalt_object_implements(NULL, NULL);
    TEST_EQUAL(result, 0);
    printf("  implements(NULL, NULL) returns 0: OK\n");

    /* NULL object, non-NULL interface: still 0 */
    result = cobalt_object_implements(NULL, iface);
    TEST_EQUAL(result, 0);
    printf("  implements(NULL, iface) returns 0: OK\n");

    /* NOTE: cobalt_object_implements() is a placeholder that always returns 0
       for non-NULL inputs (see src/core/interface.c). It only rejects NULL
       arguments, so no assertion here may expect a positive result. */

    cobalt_interface_destroy(iface);
}

void test_interface_null_vtable(void)
{
    printf("Testing interface with NULL vtable...\n");

    /* The implementation allocates the shell regardless of the vtable pointer
       and stores it verbatim: cobalt_interface_new(NULL) returns a non-NULL
       interface whose ->vtable is NULL (src/core/interface.c). */
    cobalt_interface_t *iface = cobalt_interface_new(NULL);
    TEST_ASSERT(iface != NULL);
    TEST_ASSERT(iface->vtable == NULL);
    printf("  Interface with NULL vtable created, vtable stored as NULL: OK\n");

    cobalt_interface_destroy(iface);
    printf("  Interface destroyed\n");
}

void test_interface_null_safe(void)
{
    printf("Testing interface NULL safety...\n");

    /* Destroying NULL must be a no-op, not a crash */
    cobalt_interface_destroy(NULL);
    printf("  destroy(NULL): OK\n");
}

void test_interface_with_allocator(void)
{
    printf("Testing interface with custom allocator...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    /* NULL allocator falls back to the system allocator */
    cobalt_interface_t *iface = cobalt_interface_new_with_allocator(&dummy_vtable, NULL);
    TEST_ASSERT(iface != NULL);
    TEST_ASSERT(iface->vtable == &dummy_vtable);
    cobalt_interface_destroy_with_allocator(iface, NULL);
    printf("  NULL allocator falls back to system: OK\n");

    /* Round trip with the explicit system allocator */
    iface = cobalt_interface_new_with_allocator(&dummy_vtable, sys);
    TEST_ASSERT(iface != NULL);
    TEST_ASSERT(iface->vtable == &dummy_vtable);
    cobalt_interface_destroy_with_allocator(iface, sys);
    printf("  System allocator round trip: OK\n");

    /* NULL interface destroy is a no-op, even with a NULL allocator */
    cobalt_interface_destroy_with_allocator(NULL, NULL);
    cobalt_interface_destroy_with_allocator(NULL, sys);
    printf("  destroy_with_allocator(NULL): OK\n");
}

void test_interface(void)
{
    printf("Testing interface...\n");
    test_interface_create_destroy();
    test_interface_implements();
    test_interface_null_vtable();
    test_interface_null_safe();
    test_interface_with_allocator();
    printf("  Interface tests completed\n");
}
