/**
 * @file list.c
 * @brief Implementation file of the doubly-linked list (List) container
 *
 * Implements the doubly-linked list interfaces in list.h and the cobalt_sequence_t polymorphic
 * sequence interface. Additionally, provides a specific iterator implementation for the list.
 */

#include "cobalt/container/list.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Opaque internal structure for a list node
 */
typedef struct list_node {
    void             *data; /**< Pointer to stored data */
    struct list_node *next; /**< Pointer to the next node */
    struct list_node *prev; /**< Pointer to the previous node */
} list_node_t;

/**
 * @brief Internal list structure, used to hide details, cast and inherit cobalt_sequence_t
 */
typedef struct {
    cobalt_sequence_t base; /**< Must be the first member to facilitate polymorphic casting */
    list_node_t      *head; /**< List head node */
    list_node_t      *tail; /**< List tail node */
    size_t            size; /**< Total number of nodes */
} cobalt_list_impl_t;

/* ========================================================================= */
/* Sequence Interface specific implementation                                  */
/* ========================================================================= */

/**
 * @brief Get sequence size (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Number of list elements
 */
static size_t list_size_seq(cobalt_sequence_t *self)
{
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    return list->size;
}

/**
 * @brief Check if sequence is empty (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Returns 1 if empty, otherwise 0
 */
static int list_is_empty_seq(cobalt_sequence_t *self)
{
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    return list->size == 0;
}

/**
 * @brief Add element to sequence, appends to the tail by default (Sequence interface
 * implementation)
 * @param self Sequence base class pointer
 * @param item Pointer to the element to be added
 */
static void list_add_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    list_node_t        *node = malloc(sizeof(list_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return;
    }
    node->data = item;
    node->next = NULL;
    node->prev = list->tail;

    // If the list is empty, both head and tail point to the new node
    if (!list->tail) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail       = node;
    }
    list->size++;
}

/**
 * @brief Remove element from sequence (Sequence interface implementation)
 */
static void list_remove_seq(cobalt_sequence_t *self, void *item)
{
    if (!self || !item) {
        return;
    }
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    list_node_t        *node = list->head;
    list_node_t        *prev = NULL;

    while (node) {
        if (node->data == item) {
            if (prev) {
                prev->next = node->next;
            } else {
                list->head = node->next;
            }
            if (node->next) {
                node->next->prev = prev;
            } else {
                list->tail = prev;
            }
            free(node);
            list->size--;
            return;
        }
        prev = node;
        node = node->next;
    }
}

/**
 * @brief Get sequence iterator (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Pointer to list-specific iterator
 */
static cobalt_iterator_t *list_iterator_seq(cobalt_sequence_t *self)
{
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    return cobalt_list_iterator_create((cobalt_list_t *)list);
}

/* ========================================================================= */
/* Doubly-linked list (List) public API implementation                       */
/* ========================================================================= */

/**
 * @brief Create a new doubly-linked list
 */
cobalt_list_t *cobalt_list_create(void)
{
    cobalt_list_impl_t *list = malloc(sizeof(cobalt_list_impl_t));
    if (!list) {
        return NULL;
    }

    list->head = list->tail = NULL;
    list->size              = 0;

    /* Bind polymorphic sequence interface methods */
    list->base.size     = list_size_seq;
    list->base.is_empty = list_is_empty_seq;
    list->base.add      = list_add_seq;
    list->base.remove   = list_remove_seq;
    list->base.iterator = list_iterator_seq;

    return (cobalt_list_t *)list;
}

/**
 * @brief Destroy a doubly-linked list, free all nodes and its own memory
 */
void cobalt_list_destroy(cobalt_list_t *list)
{
    if (!list) {
        return;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = impl->head;

    // Iterate through the list and free nodes one by one
    while (node) {
        list_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(list); // Free the list structure itself
}

/**
 * @brief Insert an element at the head of the list
 */
int cobalt_list_push_front(cobalt_list_t *list, void *item)
{
    if (!list) {
        return -1;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = malloc(sizeof(list_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    node->data = item;
    node->next = impl->head;
    node->prev = NULL;

    // Modify the previous pointer of the original head node
    if (impl->head) {
        impl->head->prev = node;
    }
    impl->head = node;

    // If the list was originally empty, the tail node also points to the new node
    if (!impl->tail) {
        impl->tail = node;
    }
    impl->size++;
    return 0;
}

/**
 * @brief Insert an element at the tail of the list
 */
int cobalt_list_push_back(cobalt_list_t *list, void *item)
{
    if (!list) {
        return -1;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = malloc(sizeof(list_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    node->data = item;
    node->next = NULL;
    node->prev = impl->tail;

    // If the list was originally empty, both head and tail point to the new node
    if (!impl->tail) {
        impl->head = impl->tail = node;
    } else {
        // Modify the next pointer of the original tail node
        impl->tail->next = node;
        impl->tail       = node;
    }
    impl->size++;
    return 0;
}

/**
 * @brief Remove and return the element at the head of the list
 */
void *cobalt_list_pop_front(cobalt_list_t *list)
{
    if (!list || !((cobalt_list_impl_t *)list)->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = impl->head;
    void               *data = node->data;

    // Move the head pointer forward
    impl->head = node->next;
    if (impl->head) {
        impl->head->prev = NULL;
    } else {
        // If the list is empty after removal, the tail pointer must also be nullified
        impl->tail = NULL;
    }
    free(node);
    impl->size--;
    return data;
}

/**
 * @brief Remove and return the element at the tail of the list
 */
void *cobalt_list_pop_back(cobalt_list_t *list)
{
    if (!list || !((cobalt_list_impl_t *)list)->tail) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = impl->tail;
    void               *data = node->data;

    // Move the tail pointer backward
    impl->tail = node->prev;
    if (impl->tail) {
        impl->tail->next = NULL;
    } else {
        // If the list is empty after removal, the head pointer must also be nullified
        impl->head = NULL;
    }
    free(node);
    impl->size--;
    return data;
}

/**
 * @brief Get the element at the specified index
 *
 * Adopts a bidirectionally optimized traversal method, deciding whether to traverse from the head
 * or the tail based on whether the index is in the first or second half, with a maximum time
 * complexity of O(N/2).
 */
void *cobalt_list_get(cobalt_list_t *list, size_t index)
{
    if (!list) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    if (index >= impl->size) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_BOUNDS);
        return NULL;
    }

    list_node_t *node;
    if (index < impl->size / 2) {
        // Index is in the first half, traverse forward from the head
        node = impl->head;
        for (size_t i = 0; i < index; i++) {
            node = node->next;
        }
    } else {
        // Index is in the second half, traverse backward from the tail
        node = impl->tail;
        for (size_t i = impl->size - 1; i > index; i--) {
            node = node->prev;
        }
    }
    return node->data;
}

/**
 * @brief Get the number of elements in the list
 */
size_t cobalt_list_size(cobalt_list_t *list)
{
    return list ? ((cobalt_list_impl_t *)list)->size : 0;
}

/**
 * @brief Check if the list is empty
 */
int cobalt_list_is_empty(cobalt_list_t *list)
{
    return list && ((cobalt_list_impl_t *)list)->size == 0;
}

/* ========================================================================= */
/* List Iterator implementation                                                */
/* ========================================================================= */

/**
 * @brief Iterator internal context structure, saves iteration progress
 */
typedef struct {
    cobalt_list_impl_t *list; /**< The iterated list itself (currently unused, can be used for
                                 modification validation etc.) */
    list_node_t *current;     /**< Node currently pointed to */
} list_iterator_impl_t;

/**
 * @brief Check if the iterator has a next element
 */
static int list_iterator_has_next(void *ctx)
{
    list_iterator_impl_t *impl = (list_iterator_impl_t *)ctx;
    return impl->current != NULL;
}

/**
 * @brief Get the current element of the iterator and move to the next
 */
static void *list_iterator_next(void *ctx)
{
    list_iterator_impl_t *impl = (list_iterator_impl_t *)ctx;
    if (!impl->current) {
        return NULL;
    }

    void *data    = impl->current->data;
    impl->current = impl->current->next; // Move backward
    return data;
}

/**
 * @brief Free iterator context memory
 */
static void list_iterator_destroy(void *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

/**
 * @brief List-specific iterator virtual function table
 */
static const cobalt_iterator_vtable_t list_vtable = {.has_next = list_iterator_has_next,
                                                     .next     = list_iterator_next,
                                                     .destroy  = list_iterator_destroy};

/**
 * @brief Create an exclusive iterator for traversing the doubly-linked list
 */
cobalt_iterator_t *cobalt_list_iterator_create(cobalt_list_t *list)
{
    if (!list) {
        return NULL;
    }

    cobalt_list_impl_t   *impl      = (cobalt_list_impl_t *)list;
    list_iterator_impl_t *iter_data = malloc(sizeof(list_iterator_impl_t));
    if (!iter_data) {
        return NULL;
    }

    // Initialize iterator progress
    iter_data->list    = impl;
    iter_data->current = impl->head;

    // Create common iterator structure and bind method table
    cobalt_iterator_t *iter = malloc(sizeof(cobalt_iterator_t));
    if (!iter) {
        free(iter_data);
        return NULL;
    }

    iter->vtable = &list_vtable;
    iter->data   = iter_data;
    return iter;
}

int cobalt_list_remove(cobalt_list_t *list, void *item)
{
    if (!list || !item) {
        return -1;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = impl->head;
    list_node_t        *prev = NULL;

    while (node) {
        if (node->data == item) {
            if (prev) {
                prev->next = node->next;
            } else {
                impl->head = node->next;
            }
            if (node->next) {
                node->next->prev = prev;
            } else {
                impl->tail = prev;
            }
            free(node);
            impl->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

int cobalt_list_remove_if(cobalt_list_t *list,
                          int (*predicate)(const void *item, void *user_data),
                          void *user_data)
{
    if (!list || !predicate) {
        return -1;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t        *node = impl->head;
    list_node_t        *prev = NULL;

    while (node) {
        if (predicate(node->data, user_data)) {
            if (prev) {
                prev->next = node->next;
            } else {
                impl->head = node->next;
            }
            if (node->next) {
                node->next->prev = prev;
            } else {
                impl->tail = prev;
            }
            free(node);
            impl->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}
