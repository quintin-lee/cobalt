/**
 * @file advanced_usage.c
 * @brief Demonstrate advanced Cobalt features
 */

#include <stdio.h>
#include <pthread.h>
#include "cobalt/cobalt.h"

/* Shared counter for thread test */
static cobalt_atomic_t counter = {0};

/* Thread function */
void* thread_func(void* arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        cobalt_atomic_increment(&counter);
    }
    return NULL;
}

int main(void) {
    printf("=== Advanced Usage Example ===\n\n");
    
    /* Thread-safe atomic operations */
    printf("Atomic operations:\n");
    cobalt_atomic_set(&counter, 0);
    
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_func, NULL);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int final = cobalt_atomic_get(&counter);
    printf("  Final counter: %d (expected 4000)\n", final);
    
    /* JSON parsing */
    printf("\nJSON parsing:\n");
    const char *json = "{\"name\": \"Cobalt\", \"version\": 2, \"active\": true}";
    json_node_t *root = json_parse(json);
    if (root) {
        printf("  Parsed JSON tree\n");
        json_destroy(root);
    }
    
    /* Event loop */
    printf("\nEvent loop:\n");
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (loop) {
        printf("  Event loop created\n");
        cobalt_eventloop_stop(loop);
        cobalt_eventloop_destroy(loop);
    }
    
    printf("\n=== Example completed ===\n");
    return 0;
}
