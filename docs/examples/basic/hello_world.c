/**
 * @file hello_world.c
 * @brief Simple Cobalt Hello World example demonstrating basic usage
 *
 * This example shows:
 * - Including the master header cobalt.h
 * - Platform detection
 * - Basic allocator usage
 * - Logging with macros
 */

#include <stdio.h>
#include <cobalt/cobalt.h>

int main(void) {
    /* Initialize logging to stdout at INFO level */
    FILE *log_output = stdout;
    cobalt_logger_init(log_output, LOG_LEVEL_INFO);

    cobalt_info("=== Cobalt Hello World ===\n");

    /* Get platform ID */
    cobalt_platform_id_t platform = cobalt_platform_get_id();
    cobalt_info("Platform detected: %d (1=Windows, 2=macOS, 3=Linux)\n", platform);

    /* Get system allocator */
    cobalt_allocator_t *sys_alloc = cobalt_allocator_get_system();
    if (sys_alloc) {
        cobalt_info("System allocator available\n");
    }

    cobalt_info("Hello from Cobalt framework!\n");
    return 0;
}