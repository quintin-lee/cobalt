#ifndef PLATFORM_ATOMIC_H
#define PLATFORM_ATOMIC_H

/**
 * @file atomic.h
 * @brief Platform-atomic operations using C11 stdatomic
 */

#include <stdatomic.h>
#include <stdint.h>

/* Platform-independent atomics using _Atomic */
typedef struct cobalt_atomic
{
    _Atomic int value;
} cobalt_atomic_t;

/* Atomic operations - all use appropriate memory ordering */
cobalt_atomic_t cobalt_atomic_create(int initial);
int cobalt_atomic_get(cobalt_atomic_t* a);
void cobalt_atomic_set(cobalt_atomic_t* a, int value);
void cobalt_atomic_increment(cobalt_atomic_t* a);
void cobalt_atomic_decrement(cobalt_atomic_t* a);

#endif /* PLATFORM_ATOMIC_H */
