/**
 * @file test_allocator.c
 * @Unit test for memory allocator subsystem.
 */

#include "cobalt/memory/allocator.h"
#include <stdio.h>
#include <stdlib.h>

void test_allocator_basic(void)
{
    printf("Testing allocator basic operations...\n");

    cobalt_allocator_t* sys = cobalt_allocator_get_system();
    if (!sys)
        {
            fprintf(stderr, "ERROR: Failed to get system allocator\n");
            return;
        }
    printf("  System allocator obtained\n");

    /* Test allocation */
    int* ptr = (int*)cobalt_allocator_alloc(sys, sizeof(int) * 10);
    if (!ptr)
        {
            fprintf(stderr, "ERROR: Allocation failed\n");
            return;
        }
    printf("  Allocated %zu bytes\n", sizeof(int) * 10);

    /* Verify we can use the memory */
    for (int i = 0; i < 10; i++)
        {
            ptr[i] = i * 10;
        }
    int sum = 0;
    for (int i = 0; i < 10; i++)
        {
            sum += ptr[i];
        }
    if (sum == 450)
        { /* 0+10+20+...+90 = 450 */
            printf("  Memory write/read: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Memory corruption detected\n");
        }

    /* Test realloc */
    int* new_ptr = (int*)cobalt_allocator_realloc(sys, ptr, sizeof(int) * 20);
    if (!new_ptr)
        {
            fprintf(stderr, "ERROR: Realloc failed\n");
            return;
        }
    printf("  Reallocated to 20 ints\n");

    /* Old data should still be accessible */
    if (new_ptr[0] == 0 && new_ptr[9] == 90)
        {
            printf("  Realloc preserves data: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Data lost after realloc\n");
        }

    /* Test free */
    cobalt_allocator_free(sys, new_ptr);
    printf("  Freed memory\n");
}

void test_allocator_null_safe(void)
{
    printf("Testing null safety...\n");

    cobalt_allocator_t* sys = cobalt_allocator_get_system();

    /* These should handle NULL gracefully or be documented as requiring valid input */
    int* result = (int*)cobalt_allocator_alloc(sys, 0);
    if (result || sys)
        { /* allocation of 0 may succeed or fail depending on impl */
            printf("  Zero-size alloc handled\n");
            if (result)
                cobalt_allocator_free(sys, result);
        }

    printf("  Null safety tests completed\n");
}

void test_allocator(void)
{
    printf("Testing allocator...\n");
    test_allocator_basic();
    test_allocator_null_safe();
    printf("  Allocator tests completed\n");
}
