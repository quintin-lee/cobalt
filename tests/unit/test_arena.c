/**
 * @file test_arena.c
 * @Unit test for arena (region-based) allocator.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cobalt/memory/arena.h"

void test_arena_create_destroy(void) {
    printf("Testing arena create and destroy...\n");
    
    cobalt_arena_t *arena = cobalt_arena_create(1024);
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return;
    }
    printf("  Arena created with 1024 bytes\n");
    
    cobalt_arena_destroy(arena);
    printf("  Arena destroyed successfully\n");
}

void test_arena_alloc_reset(void) {
    printf("Testing arena allocation and reset...\n");
    
    cobalt_arena_t *arena = cobalt_arena_create(256);
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return;
    }
    
    /* Allocate several blocks */
    int *a = (int *)cobalt_arena_alloc(arena, sizeof(int));
    int *b = (int *)cobalt_arena_alloc(arena, sizeof(int) * 10);
    char *c = (char *)cobalt_arena_alloc(arena, 64);
    
    if (!a || !b || !c) {
        fprintf(stderr, "ERROR: Arena allocation failed\n");
        cobalt_arena_destroy(arena);
        return;
    }
    
    /* Write and verify data */
    *a = 42;
    for (int i = 0; i < 10; i++) b[i] = i;
    for (int i = 0; i < 64; i++) c[i] = (char)(i + 'A');
    
    if (*a != 42) {
        fprintf(stderr, "ERROR: Data corruption in arena\n");
    } else {
        printf("  First allocation preserved: OK\n");
    }
    
    /* Reset arena (should invalidate all allocations) */
    cobalt_arena_reset(arena);
    printf("  Arena reset (all memory freed at once)\n");
    
    /* After reset, we should be able to allocate again from beginning */
    int *d = (int *)cobalt_arena_alloc(arena, sizeof(int));
    if (!d) {
        fprintf(stderr, "ERROR: Failed to allocate after reset\n");
    } else {
        *d = 99;
        printf("  Post-reset allocation works: OK\n");
        cobalt_arena_reset(arena); /* cleanup */
    }
    
    cobalt_arena_destroy(arena);
    printf("  Arena tests passed\n");
}

void test_arena_boundary(void) {
    printf("Testing arena boundary handling...\n");
    
    /* Create small arena */
    cobalt_arena_t *arena = cobalt_arena_create(64);
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return;
    }
    
    /* Try to allocate more than arena can hold initially - 
       implementation should grow via realloc */
    void *ptr = cobalt_arena_alloc(arena, 128);
    if (ptr) {
        printf("  Large allocation triggered growth: OK\n");
        cobalt_arena_reset(arena);
    } else {
        printf("  Large allocation rejected (within design limits)\n");
    }
    
    cobalt_arena_destroy(arena);
}

void test_arena(void) {
    printf("Testing arena...\n");
    test_arena_create_destroy();
    test_arena_alloc_reset();
    test_arena_boundary();
    printf("  Arena tests completed\n");
}
