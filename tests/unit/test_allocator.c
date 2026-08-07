/**
 * @file test_allocator.c
 * @brief Unit test for memory allocator subsystem.
 */

#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>

void test_allocator_basic(void)
{
    printf("Testing allocator basic operations...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);
    printf("  System allocator obtained\n");

    /* Test allocation */
    int *ptr = (int *)cobalt_allocator_alloc(sys, sizeof(int) * 10);
    TEST_ASSERT(ptr != NULL);
    printf("  Allocated %zu bytes\n", sizeof(int) * 10);

    /* Verify we can use the memory */
    for (int i = 0; i < 10; i++) {
        ptr[i] = i * 10;
    }
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += ptr[i];
    }
    TEST_ASSERT(sum == 450); /* 0+10+20+...+90 = 450 */
    printf("  Memory write/read: OK\n");

    /* Test realloc */
    int *new_ptr = (int *)cobalt_allocator_realloc(sys, ptr, sizeof(int) * 20);
    TEST_ASSERT(new_ptr != NULL);
    printf("  Reallocated to 20 ints\n");

    /* Old data should still be accessible */
    TEST_ASSERT(new_ptr[0] == 0 && new_ptr[9] == 90);
    printf("  Realloc preserves data: OK\n");

    /* Test free */
    cobalt_allocator_free(sys, new_ptr);
    printf("  Freed memory\n");
}

void test_allocator_null_safe(void)
{
    printf("Testing null safety...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    /* Zero-size allocation may succeed or fail depending on impl */
    int *result = (int *)cobalt_allocator_alloc(sys, 0);
    if (result) {
        cobalt_allocator_free(sys, result);
    }
    printf("  Zero-size alloc handled\n");

    /* Freeing NULL should be safe */
    cobalt_allocator_free(sys, NULL);
    printf("  Null free safe\n");
}

void test_allocator_realloc_shrink(void)
{
    printf("Testing realloc shrink...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    int *ptr = (int *)cobalt_allocator_alloc(sys, sizeof(int) * 100);
    TEST_ASSERT(ptr != NULL);

    for (int i = 0; i < 100; i++) {
        ptr[i] = i;
    }

    int *shrunk = (int *)cobalt_allocator_realloc(sys, ptr, sizeof(int) * 10);
    TEST_ASSERT(shrunk != NULL);
    TEST_ASSERT(shrunk[0] == 0);
    TEST_ASSERT(shrunk[9] == 9);
    cobalt_allocator_free(sys, shrunk);
    printf("  Realloc shrink: OK\n");
}

void test_allocator_realloc_expand(void)
{
    printf("Testing realloc expand...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    int *ptr = (int *)cobalt_allocator_alloc(sys, sizeof(int) * 10);
    TEST_ASSERT(ptr != NULL);

    for (int i = 0; i < 10; i++) {
        ptr[i] = i * 2;
    }

    int *expanded = (int *)cobalt_allocator_realloc(sys, ptr, sizeof(int) * 100);
    TEST_ASSERT(expanded != NULL);
    TEST_ASSERT(expanded[0] == 0);
    TEST_ASSERT(expanded[9] == 18);
    (void)expanded[50]; /* new region content is unspecified after realloc */
    cobalt_allocator_free(sys, expanded);
    printf("  Realloc expand: OK\n");
}

void test_allocator_zero_size(void)
{
    printf("Testing zero-size alloc...\n");

    cobalt_allocator_t *sys = cobalt_allocator_get_system();
    TEST_ASSERT(sys != NULL);

    /* alloc(0) behavior: may return NULL or a unique pointer */
    void *p0  = cobalt_allocator_alloc(sys, 0);
    void *p0b = cobalt_allocator_alloc(sys, 0);
    /* Both should be freeable without crash */
    if (p0) {
        cobalt_allocator_free(sys, p0);
    }
    if (p0b) {
        cobalt_allocator_free(sys, p0b);
    }
    printf("  Zero-size alloc: OK\n");
}

void test_allocator(void)
{
    printf("Testing allocator...\n");
    test_allocator_basic();
    test_allocator_null_safe();
    test_allocator_realloc_shrink();
    test_allocator_realloc_expand();
    test_allocator_zero_size();
    printf("  Allocator tests completed\n");
}
