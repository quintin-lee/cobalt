/**
 * @file advanced_usage.c
 * @brief Demonstrate advanced Cobalt features
 */

#include "cobalt/cobalt.h"
#include <stdio.h>

int main(void)
{
    printf("=== Advanced Usage Example ===\n\n");

    /* Thread-safe atomic operations */
    printf("Atomic operations:\n");
    cobalt_atomic_t counter = cobalt_atomic_create(0);

    cobalt_atomic_increment(&counter);
    cobalt_atomic_increment(&counter);
    cobalt_atomic_increment(&counter);

    int val = cobalt_atomic_get(&counter);
    printf("  Counter after 3 increments: %d\n", val);

    /* JSON parsing */
    printf("\nJSON parsing:\n");
    json_node_t* root = json_parse("42");
    if (root)
        {
            double num = json_get_number(root);
            printf("  Parsed number: %.1f\n", num);
            json_destroy(root);
        }

    root = json_parse("\"hello\"");
    if (root)
        {
            const char* str = json_get_string(root);
            printf("  Parsed string: %s\n", str);
            json_destroy(root);
        }

    /* Event loop */
    printf("\nEvent loop:\n");
    cobalt_eventloop_t* loop = cobalt_eventloop_create();
    if (loop)
        {
            printf("  Event loop created\n");
            cobalt_eventloop_stop(loop);
            cobalt_eventloop_destroy(loop);
            printf("  Event loop destroyed\n");
        }

    printf("\n=== Example completed ===\n");
    return 0;
}
