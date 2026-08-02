/**
 * @file atomic.c
 * @brief Implementation of atomic operations encapsulation
 * @details Implements basic functionalities such as atomic read/write, increment, and decrement based on the C11 standard `stdatomic.h`.
 */

#include "cobalt/platform/atomic.h"
#include <stdlib.h>

/*
 * @brief Create an atomic variable with an initial value
 * @details Assigns the provided initial value to the `value` field of `cobalt_atomic_t` and returns the structure.
 */
cobalt_atomic_t cobalt_atomic_create(int initial)
{
    // Initialize the built-in integer of the atomic variable
    cobalt_atomic_t a = {.value = initial};
    return a;
}

/*
 * @brief Get the current value of the atomic variable (with acquire memory order)
 * @details Uses `atomic_load_explicit` and `memory_order_acquire` to ensure that memory reads and writes after this read operation are not reordered before it.
 */
int cobalt_atomic_get(cobalt_atomic_t *a)
{
    return atomic_load_explicit(&a->value, memory_order_acquire);
}

/*
 * @brief Set the value of the atomic variable (with release memory order)
 * @details Uses `atomic_store_explicit` and `memory_order_release` to ensure that all memory reads and writes before this write operation are completed before it.
 */
void cobalt_atomic_set(cobalt_atomic_t *a, int value)
{
    atomic_store_explicit(&a->value, value, memory_order_release);
}

/*
 * @brief Atomic increment (add 1)
 * @details Uses `atomic_fetch_add_explicit` and the `memory_order_relaxed` memory order. Relaxed memory order is sufficient for counter scenarios that only care about atomicity and do not require strict memory barriers.
 */
void cobalt_atomic_increment(cobalt_atomic_t *a)
{
    atomic_fetch_add_explicit(&a->value, 1, memory_order_relaxed);
}

/*
 * @brief Atomic decrement (subtract 1)
 * @details Also uses `atomic_fetch_sub_explicit` with relaxed memory order to perform the subtraction.
 */
void cobalt_atomic_decrement(cobalt_atomic_t *a)
{
    atomic_fetch_sub_explicit(&a->value, 1, memory_order_relaxed);
}
