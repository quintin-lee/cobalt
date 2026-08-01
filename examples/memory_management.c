/**
 * @file memory_management.c
 * @brief Demonstrate arena allocator
 */

#include <stdio.h>
#include <string.h>
#include "cobalt/cobalt.h"

int main(void) {
    printf("=== Memory Management Example ===\n\n");
    
    /* Create arena */
    cobalt_arena_t *arena = cobalt_arena_create(4096);
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        return 1;
    }
    printf("Arena created (4KB)\n");
    
    /* Allocate from arena */
    int *a = cobalt_arena_alloc(arena, sizeof(int));
    double *b = cobalt_arena_alloc(arena, sizeof(double));
    char *str = cobalt_arena_alloc(arena, 100);
    
    if (a && b && str) {
        *a = 42;
        *b = 3.14;
        strcpy(str, "Hello Arena!");
        
        printf("  Allocated int: %d\n", *a);
        printf("  Allocated double: %.2f\n", *b);
        printf("  Allocated string: %s\n", str);
    }
    
    /* Reset arena - all allocations freed at once */
    cobalt_arena_reset(arena);
    printf("\nArena reset - all memory freed\n");
    
    /* Arena can be reused */
    int *c = cobalt_arena_alloc(arena, sizeof(int));
    if (c) {
        *c = 100;
        printf("  Reused arena, new int: %d\n", *c);
    }
    
    cobalt_arena_destroy(arena);
    printf("\nArena destroyed\n");
    
    printf("\n=== Example completed ===\n");
    return 0;
}
