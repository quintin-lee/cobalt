#ifndef PLATFORM_ATOMIC_H
#define PLATFORM_ATOMIC_H

/**
 * @file atomic.h
 * @brief Platform-atomic operations
 * @details Encapsulates atomic operations based on C11 `stdatomic.h`, used for thread-safe counting and state synchronization in multi-threaded environments.
 *
 * @defgroup Atomic Atomic operations module
 * @ingroup Platform
 * @{
 */

#include <stdatomic.h>
#include <stdint.h>

/**
 * @brief Platform-independent atomic variable structure
 * @details Uses the C11 `_Atomic` keyword to guarantee atomic operations on `value`.
 */
typedef struct cobalt_atomic {
    _Atomic int value; /**< The actual atomic integer value */
} cobalt_atomic_t;

/**
 * @brief Create and initialize an atomic variable
 * @param initial Initial integer value
 * @return cobalt_atomic_t The initialized atomic variable structure
 */
cobalt_atomic_t cobalt_atomic_create(int initial);

/**
 * @brief Get the current value of the atomic variable
 * @param a Pointer to the atomic variable
 * @return int The current integer value
 * @note The read operation uses the `memory_order_acquire` memory order to guarantee the latest value is fetched.
 */
int             cobalt_atomic_get(cobalt_atomic_t *a);

/**
 * @brief Set the value of the atomic variable
 * @param a Pointer to the atomic variable
 * @param value The integer value to set
 * @note The write operation uses the `memory_order_release` memory order, ensuring previous memory writes are visible to other threads.
 */
void            cobalt_atomic_set(cobalt_atomic_t *a, int value);

/**
 * @brief Increment the atomic variable (add 1)
 * @param a Pointer to the atomic variable
 * @note Uses the `memory_order_relaxed` memory order, suitable for simple counter scenarios.
 */
void            cobalt_atomic_increment(cobalt_atomic_t *a);

/**
 * @brief Decrement the atomic variable (subtract 1)
 * @param a Pointer to the atomic variable
 * @note Uses the `memory_order_relaxed` memory order, suitable for simple counter scenarios.
 */
void            cobalt_atomic_decrement(cobalt_atomic_t *a);

/** @} */

#endif /* PLATFORM_ATOMIC_H */
