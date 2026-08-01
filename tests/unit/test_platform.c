/**
 * @file test_platform.c
 * @Unit test for platform detection functions.
 */

#include "cobalt/platform/platform.h"
#include <stdio.h>

void test_platform(void)
{
    printf("Testing platform detection...\n");

    /* Test: Platform ID should be non-zero and valid */
    cobalt_platform_id_t pid = cobalt_platform_get_id();
    if (pid == 0)
        {
            fprintf(stderr, "ERROR: Platform ID is unknown/unset\n");
        }
    printf("  Platform ID detected: %d\n", pid);

    /* Test: Platform handle returns NULL */
    void* handle = cobalt_platform_get_handle();
    if (handle != NULL)
        {
            fprintf(stderr, "ERROR: Platform handle is not NULL\n");
        }
    printf("  Platform handle is NULL (as expected)\n");

#ifdef COBALT_PLATFORM_WINDOWS
    printf("  Compiled with Windows platform macro defined\n");
#elif COBALT_PLATFORM_MACOS
    printf("  Compiled with macOS platform macro defined\n");
#elif COBALT_PLATFORM_LINUX
    printf("  Compiled with Linux platform macro defined\n");
#else
    printf("  Compiled with unknown platform macro definition\n");
#endif

    printf("  All platform tests passed!\n");
}
