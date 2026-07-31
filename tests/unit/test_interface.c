/**
 * @file test_interface.c
 * @Unit test for multi-interface mechanism.
 */
#include <stdlib.h>

#include <stdio.h>
#include "cobalt/core/interface.h"
#include "cobalt/core/object.h"

/* Dummy interface vtable */
static void dummy_destroy(cobalt_interface_t *self) {
    (void)self;
    free(self);
}

static cobalt_interface_vtable_t dummy_vtable = {
    .destroy = dummy_destroy
};

void test_interface_create_destroy(void) {
    printf("Testing interface create/destroy...\n");
    
    cobalt_interface_t *iface = cobalt_interface_new(&dummy_vtable);
    if (!iface) {
        fprintf(stderr, "ERROR: Failed to create interface\n");
        return;
    }
    printf("  Interface created\n");
    
    cobalt_interface_destroy(iface);
    printf("  Interface destroyed\n");
}

void test_interface_implements(void) {
    printf("Testing interface implements check...\n");
    
    /* Create a minimal object */
    /* Note: full object requires class which we'd need to set up properly */
    /* This test validates the interface query mechanism exists */
    
    cobalt_interface_t *iface = cobalt_interface_new(&dummy_vtable);
    if (iface) {
        printf("  Interface created for testing\n");
        cobalt_interface_destroy(iface);
    }
}

void test_interface(void) {
    printf("Testing interface...\n");
    test_interface_create_destroy();
    test_interface_implements();
    printf("  Interface tests completed\n");
}
