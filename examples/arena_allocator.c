/**
 * @file arena_allocator.c
 * @brief Demonstrating memory autonomy with arena allocators
 *
 * Shows how to use arena allocation for frame-based workloads
 * where all allocations in a scope can be freed at once.
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Create a 64KB arena */
    cobalt_arena_t *arena = cobalt_arena_create(64 * 1024);
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        return 1;
    }

    cobalt_info("Created arena with 64KB buffer\n");

    /* Allocate several objects from the arena */
    int *a = (int *)cobalt_arena_alloc(arena, sizeof(int));
    int *b = (int *)cobalt_arena_alloc(arena, sizeof(int));
    int *c = (int *)cobalt_arena_alloc(arena, sizeof(int));

    if (!a || !b || !c) {
        fprintf(stderr, "Arena allocation failed\n");
        cobalt_arena_destroy(arena);
        return 1;
    }

    *a = 100;
    *b = 200;
    *c = 300;

    cobalt_info("Allocated three ints from arena: %d, %d, %d\n", *a, *b, *c);

    /* When done with this "frame", reset the arena to free everything at once */
    cobalt_arena_reset(arena);
    cobalt_info("Arena reset - all memory freed instantly!\n");

    /* The arena can now be reused for another batch of allocations */
    int *d = (int *)cobalt_arena_alloc(arena, sizeof(int));
    *d     = 400;
    cobalt_info("Reused arena, allocated new int: %d\n", *d);

    /* Finally destroy the arena itself */
    cobalt_arena_destroy(arena);

    cobalt_info("Arena allocator demo complete!\n");
    return 0;
}