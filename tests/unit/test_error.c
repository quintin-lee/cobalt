/**
 * @file test_error.c
 * @Unit test for error handling subsystem.
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

void test_error_codes(void)
{
    printf("Testing error code mappings...\n");

    cobalt_error_t codes[] = {COBALT_SUCCESS,
                              COBALT_ERROR_GENERAL,
                              COBALT_ERROR_INVALID_ARGUMENT,
                              COBALT_ERROR_OUT_OF_MEMORY,
                              COBALT_ERROR_NOT_FOUND,
                              COBALT_ERROR_ALREADY_EXISTS,
                              COBALT_ERROR_PERMISSION_DENIED,
                              COBALT_ERROR_IO,
                              COBALT_ERROR_TIMEOUT};

    const char *expected[] = {"Success",
                              "General error",
                              "Invalid argument",
                              "Out of memory",
                              "Not found",
                              "Already exists",
                              "Permission denied",
                              "IO error",
                              "Timeout"};

    int all_ok = 1;
    for (int i = 0; i < 9; i++) {
        const char *msg = cobalt_error_get_message(codes[i]);
        if (msg && strcmp(msg, expected[i]) == 0) {
            printf("  %s -> OK\n", msg);
        } else {
            fprintf(stderr,
                    "ERROR: Code %d message mismatch (got: '%s', expected: '%s')\n",
                    codes[i],
                    msg ? msg : "NULL",
                    expected[i]);
            all_ok = 0;
        }
    }

    if (all_ok) {
        printf("  All error code mappings correct\n");
    }
}

void test_error_set_get(void)
{
    printf("Testing error set/get...\n");

    cobalt_error_t err = COBALT_SUCCESS;

    /* Test setting error through pointer */
    cobalt_error_set(&err, COBALT_ERROR_NOT_FOUND);
    if (err == COBALT_ERROR_NOT_FOUND) {
        printf("  Error set via pointer: OK\n");
    } else {
        fprintf(stderr, "ERROR: Error not set correctly\n");
    }

    /* Test passing NULL (should be safe) */
    cobalt_error_set(NULL, COBALT_ERROR_IO);
    printf("  NULL error pointer handled safely\n");

    /* Test current error (implementation returns SUCCESS as placeholder) */
    cobalt_error_t current = cobalt_error_get_current();
    printf("  Current thread-local error: %d (0=SUCCESS)\n", current);
}

void test_error_thread_local(void)
{
    printf("Testing thread-local error tracking...\n");

    cobalt_error_set(NULL, COBALT_SUCCESS);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_SUCCESS);

    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_NOT_FOUND);

    cobalt_error_set(NULL, COBALT_SUCCESS);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_SUCCESS);

    printf("  Thread-local error tracking test passed\n");
}

void test_error_hashmap_integration(void)
{
    printf("Testing hashmap error integration...\n");

    cobalt_error_set(NULL, COBALT_SUCCESS);

    cobalt_hashmap_t *map = cobalt_hashmap_create(8);
    TEST_ASSERT(map != NULL);

    TEST_ASSERT(cobalt_error_get_current() == COBALT_SUCCESS);

    void *val = cobalt_hashmap_get(map, "missing");
    TEST_ASSERT(val == NULL);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_NOT_FOUND);

    TEST_ASSERT(cobalt_hashmap_remove(map, "missing") == -1);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_NOT_FOUND);

    TEST_ASSERT(cobalt_hashmap_put(map, NULL, NULL) == -1);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_INVALID_ARGUMENT);

    cobalt_hashmap_destroy(map);
    printf("  Hashmap error integration test passed\n");
}

void test_error(void)
{
    printf("Testing error...\n");
    test_error_codes();
    test_error_set_get();
    test_error_thread_local();
    test_error_hashmap_integration();
    printf("  Error tests completed\n");
}

