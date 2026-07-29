#ifndef PLATFORM_ATOMIC_H
#define PLATFORM_ATOMIC_H

/**
 * @file atomic.h
 * @brief Platform-atomic operations
 */

/* Platform-independent atomics */
typedef struct cobalt_atomic { int value; } cobalt_atomic_t;

cobalt_atomic_t cobalt_atomic_create(int initial);
int cobalt_atomic_get(cobalt_atomic_t *a);
void cobalt_atomic_set(cobalt_atomic_t *a, int value);
void cobalt_atomic_increment(cobalt_atomic_t *a);
void cobalt_atomic_decrement(cobalt_atomic_t *a);

#endif /* PLATFORM_ATOMIC_H */
