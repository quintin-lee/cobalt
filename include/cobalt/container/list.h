#ifndef LIST_H
#define LIST_H

/**
 * @file list.h
 * @brief Doubly-linked List container
 *
 * Provides a sequence container based on a doubly-linked list, supporting efficient insertion and
 * deletion operations at both ends. Also implements the cobalt_sequence_t interface, allowing
 * polymorphic sequence operations.
 */

#include "cobalt/interface/sequence.h"

/**
 * @brief Forward declaration of list node type
 */
typedef struct cobalt_list_node cobalt_list_node_t;

/**
 * @brief Forward declaration of doubly-linked list type
 */
typedef struct cobalt_list cobalt_list_t;

/**
 * @defgroup list_api Doubly-linked list operation interfaces
 * @{
 */

/**
 * @brief Create a new doubly-linked list
 *
 * @return Returns a pointer to the newly created list on success; returns NULL if memory allocation
 * fails.
 */
cobalt_list_t *cobalt_list_create(void);

/**
 * @brief Destroy a doubly-linked list and free its memory
 *
 * Traverses the entire list, frees the memory of all nodes, and finally frees the memory of the
 * list structure itself.
 * @param list Pointer to the list to be destroyed
 * @warning This operation only frees the memory of nodes and the list container itself; it does not
 * free the user data memory pointed to by the data pointers.
 */
void cobalt_list_destroy(cobalt_list_t *list);

/**
 * @brief Insert an element at the head of the list
 *
 * @param list Pointer to the target list
 * @param item Pointer to the data to be inserted
 * @return Returns 0 on success; returns -1 if list is NULL or memory allocation fails.
 */
int cobalt_list_push_front(cobalt_list_t *list, void *item);

/**
 * @brief Insert an element at the tail of the list
 *
 * @param list Pointer to the target list
 * @param item Pointer to the data to be inserted
 * @return Returns 0 on success; returns -1 if list is NULL or memory allocation fails.
 */
int cobalt_list_push_back(cobalt_list_t *list, void *item);

/**
 * @brief Remove and return the element at the head of the list
 *
 * @param list Pointer to the target list
 * @return Data pointer stored in the original head node of the list; returns NULL if the list is
 * empty.
 */
void *cobalt_list_pop_front(cobalt_list_t *list);

/**
 * @brief Remove and return the element at the tail of the list
 *
 * @param list Pointer to the target list
 * @return Data pointer stored in the original tail node of the list; returns NULL if the list is
 * empty.
 */
void *cobalt_list_pop_back(cobalt_list_t *list);

/**
 * @brief Get the element at the specified index
 *
 * Internally automatically determines whether to traverse from the head or the tail to shorten the
 * search path (i.e., traverses at most half the length).
 * @param list Pointer to the target list
 * @param index The index of the target element (starting from 0)
 * @return Returns the data pointer at the specified index on success; returns NULL if the index is
 * out of bounds or the list is empty.
 */
void *cobalt_list_get(const cobalt_list_t *list, size_t index);

/**
 * @brief Get the number of elements in the list
 *
 * @param list Pointer to the target list
 * @return The number of elements in the list; if list is NULL, returns 0.
 */
size_t cobalt_list_size(const cobalt_list_t *list);

/**
 * @brief Check if the list is empty
 *
 * @param list Pointer to the target list
 * @return Returns 1 if the list is empty (length is 0); otherwise returns 0. If list is NULL, also
 * returns 0.
 */
int cobalt_list_is_empty(const cobalt_list_t *list);

/**
 * @brief Create a specific iterator for the list
 *
 * Returns an iterator for traversing all elements in the list in the forward direction.
 * @param list Pointer to the target list
 * @return Returns an iterator pointer on success; returns NULL on failure or if list is NULL.
 */
cobalt_iterator_t *cobalt_list_iterator_create(cobalt_list_t *list);

/**
 * @brief Remove the first occurrence of an element from the list
 *
 * @param list Pointer to the target list
 * @param item Pointer to the data to remove (compared by pointer equality)
 * @return Returns 0 on success; returns -1 if list is NULL, item is NULL, or element not found.
 */
int cobalt_list_remove(cobalt_list_t *list, void *item);

/**
 * @brief Remove the first element for which the predicate returns non-zero
 *
 * @param list Pointer to the target list
 * @param predicate Function that returns non-zero for elements to remove
 * @param user_data Opaque pointer passed to the predicate
 * @return Returns 0 on success; returns -1 if list is NULL, predicate is NULL, or no element matches.
 */
int cobalt_list_remove_if(cobalt_list_t *list,
                          int (*predicate)(const void *item, void *user_data),
                          void *user_data);

/** @} */

#endif /* LIST_H */
