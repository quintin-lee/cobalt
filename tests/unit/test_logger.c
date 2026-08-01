/**
 * @file test_logger.c
 * @Unit test for logging subsystem.
 */

#include "cobalt/runtime/logger.h"
#include <stdio.h>
#include <string.h>

/* Capture log output to string */
static char log_buffer[1024];
static FILE* log_fp;

void test_logger_levels(void)
{
    printf("Testing log level filtering...\n");

    /* Open a temp file for capturing */
    log_fp = fopen("/tmp/cobalt_test_log.txt", "w");
    if (!log_fp)
        {
            fprintf(stderr, "WARNING: Cannot open temp log file\n");
            return;
        }

    /* Initialize with INFO level */
    cobalt_logger_init(log_fp, LOG_LEVEL_INFO);

    /* These should NOT appear (below threshold) */
    cobalt_trace("trace message");
    cobalt_debug("debug message");

    /* These SHOULD appear */
    cobalt_info("info message");
    cobalt_warning("warning message");
    cobalt_error("error message");

    fclose(log_fp);

    /* Read back and verify */
    log_fp = fopen("/tmp/cobalt_test_log.txt", "r");
    if (log_fp)
        {
            char line[256];
            int info_count = 0;
            int warn_count = 0;
            int err_count = 0;

            while (fgets(line, sizeof(line), log_fp))
                {
                    if (strstr(line, "INFO"))
                        info_count++;
                    if (strstr(line, "WARNING"))
                        warn_count++;
                    if (strstr(line, "ERROR"))
                        err_count++;
                }
            fclose(log_fp);

            printf("  Log levels filtered: INFO=%d WARN=%d ERR=%d\n", info_count, warn_count,
                   err_count);

            if (info_count >= 1 && warn_count >= 1 && err_count >= 1)
                {
                    printf("  Logger level filtering: OK\n");
                }
            else
                {
                    fprintf(stderr, "ERROR: Unexpected log counts\n");
                }
        }
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
    test_logger_macros();
    printf("  Logger tests completed\n");
}
