#include "cobalt/container/vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(void)
{
    printf("Vector Benchmark\n");
    printf("================\n");
    
    /* Test 1: Push 1M elements */
    cobalt_vector_t* vec = cobalt_vector_create(1024);
    int value = 42;
    
    double start = current_time_ms();
    for (int i = 0; i < 1000000; i++)
    {
        cobalt_vector_push(vec, &value);
    }
    double push_time = current_time_ms() - start;
    printf("Push 1M elements: %.2f ms\n", push_time);
    
    /* Test 2: Get 1M elements */
    start = current_time_ms();
    for (int i = 0; i < 1000000; i++)
    {
        cobalt_vector_get(vec, i);
    }
    double get_time = current_time_ms() - start;
    printf("Get 1M elements: %.2f ms\n", get_time);
    
    cobalt_vector_destroy(vec);
    
    printf("Benchmark completed\n");
    return 0;
}
