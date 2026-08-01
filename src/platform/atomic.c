#include "cobalt/platform/atomic.h"
#include <stdlib.h>

/* Create a new atomic with initial value */
cobalt_atomic_t cobalt_atomic_create(int initial)
{
    cobalt_atomic_t a = {.value = initial};
    return a;
}

/* Get current value (acquire ordering for reading) */
int cobalt_atomic_get(cobalt_atomic_t* a)
{
    return atomic_load_explicit(&a->value, memory_order_acquire);
}

/* Set value (release ordering for writing) */
void cobalt_atomic_set(cobalt_atomic_t* a, int value)
{
    atomic_store_explicit(&a->value, value, memory_order_release);
}

/* Increment by 1 (fetch-and-add, relaxed ordering sufficient for simple counters) */
void cobalt_atomic_increment(cobalt_atomic_t* a)
{
    atomic_fetch_add_explicit(&a->value, 1, memory_order_relaxed);
}

/* Decrement by 1 (fetch-and-subtract) */
void cobalt_atomic_decrement(cobalt_atomic_t* a)
{
    atomic_fetch_sub_explicit(&a->value, 1, memory_order_relaxed);
}
