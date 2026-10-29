/**
 * @file test_logger.c
 * @Unit test for logging subsystem.
 */

#include "cobalt/runtime/logger.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

/* Capture log output to string */
static FILE *log_fp;

void test_logger_levels(void)
{
    printf("Testing log level filtering...\n");

    /* Open an unnamed temp file for capturing (portable: no /tmp path) */
    log_fp = cobalt_test_tmpfile();
    TEST_ASSERT(log_fp != NULL);

    /* Initialize with INFO level */
    cobalt_logger_init(log_fp, LOG_LEVEL_INFO);

    /* These should NOT appear (below threshold) */
    cobalt_trace("trace message");
    cobalt_debug("debug message");

    /* These SHOULD appear */
    cobalt_info("info message");
    cobalt_warning("warning message");
    cobalt_error("error message");

    /* tmpfile() cannot be reopened: flush then rewind the same handle */
    TEST_ASSERT(fflush(log_fp) == 0);
    rewind(log_fp);

    char line[256];
    int  info_count  = 0;
    int  warn_count  = 0;
    int  err_count   = 0;
    int  trace_count = 0;
    int  debug_count = 0;
    int  total_lines = 0;

    while (fgets(line, sizeof(line), log_fp)) {
        total_lines++;
        if (strstr(line, "TRACE")) {
            trace_count++;
        }
        if (strstr(line, "DEBUG")) {
            debug_count++;
        }
        if (strstr(line, "INFO")) {
            info_count++;
        }
        if (strstr(line, "WARNING")) {
            warn_count++;
        }
        if (strstr(line, "ERROR")) {
            err_count++;
        }
    }
    TEST_ASSERT(fclose(log_fp) == 0);
    log_fp = NULL;

    printf("  Log levels filtered: INFO=%d WARN=%d ERR=%d\n", info_count, warn_count, err_count);

    TEST_EQUAL(trace_count, 0); /* filtered: below threshold */
    TEST_EQUAL(debug_count, 0); /* filtered: below threshold */
    TEST_EQUAL(info_count, 1);
    TEST_EQUAL(warn_count, 1);
    TEST_EQUAL(err_count, 1);
    TEST_EQUAL(total_lines, 3);
    printf("  Logger level filtering: OK\n");

    /* Leave the logger in a deterministic state for the remaining tests */
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);
}

void test_logger_null_stream(void)
{
    printf("Testing logger with NULL output stream...\n");

    /* cobalt_logger_init() with NULL output falls back to stdout, so logging
       must not crash. Output goes to the test's own stdout. */
    cobalt_logger_init(NULL, LOG_LEVEL_ERROR);
    cobalt_error("null stream test");
    printf("  NULL stream falls back to stdout: OK\n");

    /* Restore a deterministic state for later tests */
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);
}

void test_logger_level_reset(void)
{
    printf("Testing logger level re-set mid-run...\n");

    FILE *fp = cobalt_test_tmpfile();
    TEST_ASSERT(fp != NULL);

    /* Raise the threshold to ERROR: info/warning must be suppressed */
    cobalt_logger_init(fp, LOG_LEVEL_ERROR);
    cobalt_trace("reset trace");
    cobalt_debug("reset debug");
    cobalt_info("reset info");
    cobalt_warning("reset warning");
    cobalt_error("reset error");

    TEST_ASSERT(fflush(fp) == 0);
    rewind(fp);

    char line[256];
    int  total       = 0;
    int  info_count  = 0;
    int  warn_count  = 0;
    int  trace_count = 0;
    int  debug_count = 0;
    int  err_count   = 0;

    while (fgets(line, sizeof(line), fp)) {
        total++;
        if (strstr(line, "TRACE")) {
            trace_count++;
        }
        if (strstr(line, "DEBUG")) {
            debug_count++;
        }
        if (strstr(line, "INFO")) {
            info_count++;
        }
        if (strstr(line, "WARNING")) {
            warn_count++;
        }
        if (strstr(line, "ERROR")) {
            err_count++;
        }
    }

    /* min_log_level is mutable: resetting it to ERROR suppresses everything below,
       and accepts ERROR. */
    TEST_EQUAL(trace_count, 0);
    TEST_EQUAL(debug_count, 0);
    TEST_EQUAL(info_count, 0);
    TEST_EQUAL(warn_count, 0);
    TEST_EQUAL(err_count, 1);
    TEST_EQUAL(total, 1);
    printf("  Level re-set to ERROR suppresses lower levels: OK\n");

    /* Lower the threshold again to prove the reset is not one-way */
    TEST_ASSERT(fclose(fp) == 0);
    fp = cobalt_test_tmpfile();
    TEST_ASSERT(fp != NULL);
    cobalt_logger_init(fp, LOG_LEVEL_TRACE);
    cobalt_trace("reset trace 2");

    TEST_ASSERT(fflush(fp) == 0);
    rewind(fp);
    int trace_found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "TRACE")) {
            trace_found = 1;
        }
    }
    TEST_ASSERT(fclose(fp) == 0);
    TEST_EQUAL(trace_found, 1);
    printf("  Level re-set to TRACE re-enables trace: OK\n");

    /* Restore a deterministic state for the remaining tests */
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);
}

void test_logger_macros(void)
{
    printf("Testing logger macros (file/line injection)...\n");

    /* Reinitialize to stdout for visible output */
    cobalt_logger_init(stdout, LOG_LEVEL_TRACE);

    /* Each macro should inject __FILE__ and __LINE__ automatically */
    cobalt_trace("trace test");
    cobalt_debug("debug test");
    cobalt_info("info test");
    cobalt_warning("warning test");
    cobalt_error("error test");

    /* Fatal should terminate - we can't easily test this without forking,
       so we just verify the macro exists and compiles */
    printf("  Logger macros compile and execute\n");
}

void test_logger(void)
{
    printf("Testing logger...\n");
    test_logger_levels();
    test_logger_null_stream();
    test_logger_level_reset();
    test_logger_macros();
    printf("  Logger tests completed\n");
}
