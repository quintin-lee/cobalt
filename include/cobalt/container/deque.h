#ifndef DEQUE_H
#define DEQUE_H

/**
 * @file deque.h
 * @brief Double-ended queue (Deque) container
 * 
 * A double-ended queue is a linear data structure that allows insertion and deletion operations at both ends.
 * This implementation is based on a doubly-linked list.
 */

#include <stddef.h>

/**
 * @brief Forward declaration of opaque double-ended queue type
 */
typedef struct cobalt_deque cobalt_deque_t;

/**
 * @defgroup deque_api Double-ended queue operation interfaces
 * @{
 */

/**
 * @brief Create a new double-ended queue
 * 
 * @return Returns a pointer to the newly created double-ended queue on success; returns NULL if memory allocation fails.
 */
cobalt_deque_t *cobalt_deque_create(void);

/**
 * @brief Destroy a double-ended queue and free its memory
 * 
 * Frees the memory of all nodes in the queue as well as the memory of the queue container itself.
 * @param deque Pointer to the double-ended queue to be destroyed
 * @warning This operation does not free the memory of the user data itself stored in the queue nodes.
 */
void cobalt_deque_destroy(cobalt_deque_t *deque);

/**
 * @brief Insert an element at the front of the double-ended queue
 * 
 * @param deque Pointer to the target double-ended queue
 * @param item Pointer to the user data to be inserted
 * @return Returns 0 on success; returns -1 if deque is NULL or memory allocation fails.
 */
int cobalt_deque_push_front(cobalt_deque_t *deque, void *item);

/**
 * @brief Insert an element at the back of the double-ended queue
 * 
 * @param deque Pointer to the target double-ended queue
 * @param item Pointer to the user data to be inserted
 * @return Returns 0 on success; returns -1 if deque is NULL or memory allocation fails.
 */
int cobalt_deque_push_back(cobalt_deque_t *deque, void *item);

/**
 * @brief Remove and return the element at the front of the double-ended queue
 * 
 * @param deque Pointer to the target double-ended queue
 * @return User data pointer stored in the original front node; returns NULL if the queue is empty or deque is NULL.
 */
void *cobalt_deque_pop_front(cobalt_deque_t *deque);

/**
 * @brief Remove and return the element at the back of the double-ended queue
 * 
 * @param deque Pointer to the target double-ended queue
 * @return User data pointer stored in the original back node; returns NULL if the queue is empty or deque is NULL.
 */
void *cobalt_deque_pop_back(cobalt_deque_t *deque);

/**
 * @brief Get the element at the front of the double-ended queue (without removing it)
 * 
 * @param deque Pointer to the target double-ended queue
 * @return User data pointer stored in the front node; returns NULL if the queue is empty or deque is NULL.
 */
void *cobalt_deque_peek_front(cobalt_deque_t *deque);

/**
 * @brief Get the element at the back of the double-ended queue (without removing it)
 * 
 * @param deque Pointer to the target double-ended queue
 * @return User data pointer stored in the back node; returns NULL if the queue is empty or deque is NULL.
 */
void *cobalt_deque_peek_back(cobalt_deque_t *deque);

/**
 * @brief Get the number of elements in the double-ended queue
 * 
 * @param deque Pointer to the target double-ended queue
 * @return The number of elements in the double-ended queue; returns 0 if deque is NULL.
 */
size_t cobalt_deque_size(cobalt_deque_t *deque);

/**
 * @brief Check if the double-ended queue is empty
 * 
 * @param deque Pointer to the target double-ended queue
 * @return Returns 1 if the queue is empty; otherwise returns 0. If deque is NULL, also returns 1.
 */
int cobalt_deque_is_empty(cobalt_deque_t *deque);

/** @} */

#endif /* DEQUE_H */
