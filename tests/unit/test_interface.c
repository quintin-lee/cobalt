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
    
    cobalt_interface_t *iface = cobalt_interface_new(&dummy_vtable);
    if (iface) {
        printf("  Interface created for testing\n");
        
        /* NULL checks */
        int result = cobalt_object_implements(NULL, NULL);
        if (result == 0) {
            printf("  implements(NULL, NULL) returns 0: OK\n");
        }
        
        cobalt_interface_destroy(iface);
    }
}

void test_interface_null_vtable(void) {
    printf("Testing interface with NULL vtable...\n");
    
    cobalt_interface_t *iface = cobalt_interface_new(NULL);
    if (iface) {
        printf("  Interface with NULL vtable created\n");
        cobalt_interface_destroy(iface);
    } else {
        printf("  Interface with NULL vtable returns NULL: OK\n");
    }
}

void test_interface(void) {
    printf("Testing interface...\n");
    test_interface_create_destroy();
    test_interface_implements();
    test_interface_null_vtable();
    printf("  Interface tests completed\n");
}
