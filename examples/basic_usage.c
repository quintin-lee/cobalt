/**
 * @file basic_usage.c
 * @brief Demonstrate basic Cobalt framework usage
 */

#include "cobalt/cobalt.h"
#include <stdio.h>

int main(void)
{
    printf("=== Cobalt Framework Example ===\n\n");

    /* Platform detection */
    printf("Platform: %d (1=Windows, 2=macOS, 3=Linux)\n", cobalt_platform_get_id());

    /* Allocator */
    cobalt_allocator_t* alloc = cobalt_allocator_get_system();
    int* ptr = cobalt_allocator_alloc(alloc, sizeof(int) * 10);
    if (ptr)
        {
            printf("Allocator: OK\n");
            cobalt_allocator_free(alloc, ptr);
        }

    /* Arena */
    cobalt_arena_t* arena = cobalt_arena_create(1024);
    if (arena)
        {
            void* mem = cobalt_arena_alloc(arena, 64);
            printf("Arena: OK\n");
            cobalt_arena_destroy(arena);
        }

    /* Error handling */
    cobalt_error_t err = COBALT_SUCCESS;
    cobalt_error_set(&err, COBALT_ERROR_NOT_FOUND);
    printf("Error: %s\n", cobalt_error_get_message(err));

    /* Logger */
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);
    cobalt_info("Hello from Cobalt!");

    /* Vector */
    cobalt_vector_t* vec = cobalt_vector_create(5);
    int val = 42;
    cobalt_vector_push(vec, &val);
    printf("Vector size: %zu\n", cobalt_vector_size(vec));
    cobalt_vector_destroy(vec);

    /* Hashmap */
    cobalt_hashmap_t* map = cobalt_hashmap_create(16);
    cobalt_hashmap_put(map, "test", &val);
    int* got = (int*)cobalt_hashmap_get(map, "test");
    if (got)
        printf("Hashmap get: %d\n", *got);
    cobalt_hashmap_destroy(map);

    printf("\n=== Example completed ===\n");
    return 0;
}
