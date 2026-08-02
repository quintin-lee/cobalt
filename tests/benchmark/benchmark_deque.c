#include "cobalt/container/deque.h"
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
    printf("Deque Benchmark\n");
    printf("===============\n");

    cobalt_deque_t* dq = cobalt_deque_create();
    if (!dq) {
        fprintf(stderr, "Failed to create deque\n");
        return 1;
    }

    int value = 42;

    /* Test 1: Push back 1M elements */
    double start = current_time_ms();
    for (int i = 0; i < 1000000; i++) {
        cobalt_deque_push_back(dq, &value);
    }
    double push_back_time = current_time_ms() - start;
    printf("Push back 1M elements: %.2f ms\n", push_back_time);

    /* Test 2: Push front 1M elements */
    start = current_time_ms();
    for (int i = 0; i < 1000000; i++) {
        cobalt_deque_push_front(dq, &value);
    }
    double push_front_time = current_time_ms() - start;
    printf("Push front 1M elements: %.2f ms\n", push_front_time);

    /* Test 3: Pop front 2M elements */
    start = current_time_ms();
    for (int i = 0; i < 2000000; i++) {
        cobalt_deque_pop_front(dq);
    }
    double pop_front_time = current_time_ms() - start;
    printf("Pop front 2M elements: %.2f ms\n", pop_front_time);

    /* Test 4: Pop back remaining */
    start = current_time_ms();
    while (cobalt_deque_size(dq) > 0) {
        cobalt_deque_pop_back(dq);
    }
    double pop_back_time = current_time_ms() - start;
    printf("Pop back remaining: %.2f ms\n", pop_back_time);

    cobalt_deque_destroy(dq);

    printf("Benchmark completed\n");
    return 0;
}
