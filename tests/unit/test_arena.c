/**
 * @file test_arena.c
 * @Unit test for arena (region-based) allocator.
 */
#include "cobalt/memory/arena.h"
#include "test_framework.h"
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_arena_create_destroy(void)
{
    printf("Testing arena create and destroy...\n");

    cobalt_arena_t *arena = cobalt_arena_create(1024);
    TEST_ASSERT(arena != NULL);
    printf("  Arena created with 1024 bytes\n");

    cobalt_arena_destroy(arena);
    printf("  Arena destroyed successfully\n");
}

void test_arena_alloc_reset(void)
{
    printf("Testing arena allocation and reset...\n");

    cobalt_arena_t *arena = cobalt_arena_create(256);
    TEST_ASSERT(arena != NULL);

    /* Allocate several blocks */
    int  *a = (int *)cobalt_arena_alloc(arena, sizeof(int));
    int  *b = (int *)cobalt_arena_alloc(arena, sizeof(int) * 10);
    char *c = (char *)cobalt_arena_alloc(arena, 64);

    TEST_ASSERT(a != NULL);
    TEST_ASSERT(b != NULL);
    TEST_ASSERT(c != NULL);

    /* Write and verify data */
    *a = 42;
    for (int i = 0; i < 10; i++) {
        b[i] = i;
    }
    for (int i = 0; i < 64; i++) {
        c[i] = (char)(i + 'A');
    }

    TEST_EQUAL(*a, 42);

    /* Reset arena (should invalidate all allocations) */
    cobalt_arena_reset(arena);
    printf("  Arena reset (all memory freed at once)\n");

    /* After reset, we should be able to allocate again from beginning */
    int *d = (int *)cobalt_arena_alloc(arena, sizeof(int));
    TEST_ASSERT(d != NULL);
    *d = 99;
    printf("  Post-reset allocation works: OK\n");
    cobalt_arena_reset(arena); /* cleanup */

    cobalt_arena_destroy(arena);
    printf("  Arena tests passed\n");
}

void test_arena_boundary(void)
{
    printf("Testing arena boundary handling...\n");

    /* Create small arena */
    cobalt_arena_t *arena = cobalt_arena_create(64);
    TEST_ASSERT(arena != NULL);

    /* Try to allocate more than arena can hold initially -
       implementation should grow via realloc */
    void *ptr = cobalt_arena_alloc(arena, 128);
    TEST_ASSERT(ptr != NULL);
    printf("  Large allocation triggered growth: OK\n");
    cobalt_arena_reset(arena);

    cobalt_arena_destroy(arena);
}

void test_arena_alignment(void)
{
    printf("Testing arena alignment...\n");

    cobalt_arena_t *arena = cobalt_arena_create(1024);
    TEST_ASSERT(arena != NULL);

    size_t alignment = alignof(max_align_t);

    for (size_t sz = 1; sz <= 64; sz++) {
        void *ptr = cobalt_arena_alloc(arena, sz);
        TEST_ASSERT(ptr != NULL);
        TEST_ASSERT(((uintptr_t)ptr % alignment) == 0);
    }

    printf("  Arena alignment test passed\n");
    cobalt_arena_destroy(arena);
}

void test_arena_pointer_stability(void)
{
    printf("Testing arena pointer stability...\n");

    /* Small initial capacity to force multiple chunk allocations */
    cobalt_arena_t *arena = cobalt_arena_create(128);
    TEST_ASSERT(arena != NULL);

#define NUM_SLICES 64
#define SLICE_SIZE 64
    void *ptrs[NUM_SLICES];

    for (int i = 0; i < NUM_SLICES; i++) {
        ptrs[i] = cobalt_arena_alloc(arena, SLICE_SIZE);
        TEST_ASSERT(ptrs[i] != NULL);
        memset(ptrs[i], (i & 0xFF), SLICE_SIZE);
    }

    /* Verify all pointers remain stable and data is not corrupted by subsequent allocations */
    for (int i = 0; i < NUM_SLICES; i++) {
        unsigned char *bytes = (unsigned char *)ptrs[i];
        for (int j = 0; j < SLICE_SIZE; j++) {
            TEST_ASSERT(bytes[j] == (unsigned char)(i & 0xFF));
        }
    }

    cobalt_arena_reset(arena);
    void *new_ptr = cobalt_arena_alloc(arena, 64);
    TEST_ASSERT(new_ptr != NULL);

    cobalt_arena_destroy(arena);
#undef NUM_SLICES
#undef SLICE_SIZE
    printf("  Arena pointer stability test passed\n");
}

void test_arena_with_allocator(void)
{
    printf("Testing arena with custom allocator...\n");

    /* Verified against arena.c: NULL allocator is rejected, not defaulted */
    TEST_ASSERT(cobalt_arena_create_with_allocator(1024, NULL) == NULL);

    cobalt_test_counting_init();
    cobalt_arena_t *arena = cobalt_arena_create_with_allocator(1024, &cobalt_test_counting_v);
    TEST_ASSERT(arena != NULL);
    TEST_ASSERT(cobalt_test_count_allocs > 0); /* arena came from the wrapper */

    void *p1 = cobalt_arena_alloc(arena, 64);
    void *p2 = cobalt_arena_alloc(arena, 128);
    TEST_ASSERT(p1 != NULL);
    TEST_ASSERT(p2 != NULL);

    /* Probe-verified: the arena pre-bundles its chunk, so inner allocations
       do not add allocator traffic. */
    TEST_EQUAL(cobalt_test_count_allocs, 2);

    cobalt_arena_destroy(arena);
    TEST_EQUAL(cobalt_test_count_frees, 2); /* chunk + arena, both via wrapper */
    printf("  Arena allocator injection boundary: OK\n");
}

void test_arena(void)
{
    printf("Testing arena...\n");
    test_arena_create_destroy();
    test_arena_alloc_reset();
    test_arena_boundary();
    test_arena_alignment();
    test_arena_pointer_stability();
    test_arena_with_allocator();
    printf("  Arena tests completed\n");
}
