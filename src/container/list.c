/**
 * @file list.c
 * @brief Implementation file of the doubly-linked list (List) container
 *
 * Implements the doubly-linked list interfaces in list.h and the cobalt_sequence_t polymorphic
 * sequence interface. Additionally, provides a specific iterator implementation for the list.
 */

#include "cobalt/container/list.h"
#include "cobalt/algorithm/sort.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Opaque list node structure
 */
struct cobalt_list_node {
    void               *data; /**< Pointer to stored data */
    cobalt_list_node_t *next; /**< Pointer to the next node */
    cobalt_list_node_t *prev; /**< Pointer to the previous node */
};

/**
 * @brief Opaque doubly-linked list structure
 * @note Inherits from the cobalt_sequence_t interface, supporting polymorphic sequence operations.
 */
struct cobalt_list {
    cobalt_sequence_t   base; /**< Base sequence interface (for polymorphism) */
    cobalt_allocator_t *alloc;
    cobalt_list_node_t *head; /**< Pointer to the list head node */
    cobalt_list_node_t *tail; /**< Pointer to the list tail node */
    size_t              size; /**< Number of elements currently stored in the list */
};

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
    cobalt_list_t *list = (cobalt_list_t *)self;
    return list->size;
}

/**
 * @brief Check if sequence is empty (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Returns 1 if empty, otherwise 0
 */
static int list_is_empty_seq(cobalt_sequence_t *self)
{
    cobalt_list_t *list = (cobalt_list_t *)self;
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
    cobalt_list_t      *list = (cobalt_list_t *)self;
    cobalt_list_node_t *node =
        (cobalt_list_node_t *)list->alloc->alloc(list->alloc, sizeof(cobalt_list_node_t));
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
    cobalt_list_t      *list = (cobalt_list_t *)self;
    cobalt_list_node_t *node = list->head;
    cobalt_list_node_t *prev = NULL;

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
            list->alloc->free(list->alloc, node);
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
static void *list_get_at_index_seq(cobalt_sequence_t *self, size_t index)
{
    cobalt_list_t *list = (cobalt_list_t *)self;
    return cobalt_list_get(list, index);
}

static cobalt_iterator_t *list_iterator_seq(cobalt_sequence_t *self)
{
    cobalt_list_t *list = (cobalt_list_t *)self;
    return cobalt_list_iterator_create(list);
}

/* ========================================================================= */
/* Doubly-linked list (List) public API implementation                       */
/* ========================================================================= */

/**
 * @brief Create a new doubly-linked list
 */
cobalt_list_t *cobalt_list_create(void)
{
    return cobalt_list_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_list_t *cobalt_list_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_list_t *list = (cobalt_list_t *)alloc->alloc(alloc, sizeof(cobalt_list_t));
    if (!list) {
        return NULL;
    }
    list->head = list->tail = NULL;
    list->size              = 0;
    list->alloc             = alloc;

    /* Bind polymorphic sequence interface methods */
    list->base.size         = list_size_seq;
    list->base.is_empty     = list_is_empty_seq;
    list->base.add          = list_add_seq;
    list->base.remove       = list_remove_seq;
    list->base.iterator     = list_iterator_seq;
    list->base.get_at_index = list_get_at_index_seq;

    return list;
}

/**
 * @brief Destroy a doubly-linked list, free all nodes and its own memory
 */
void cobalt_list_destroy(cobalt_list_t *list)
{
    if (!list) {
        return;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node = impl->head;

    while (node) {
        cobalt_list_node_t *next = node->next;
        list->alloc->free(list->alloc, node);
        node = next;
    }
    list->alloc->free(list->alloc, list);
}

/**
 * @brief Insert an element at the head of the list
 */
int cobalt_list_push_front(cobalt_list_t *list, void *item)
{
    if (!list) {
        return -1;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node =
        (cobalt_list_node_t *)list->alloc->alloc(list->alloc, sizeof(cobalt_list_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    node->data = item;
    node->next = impl->head;
    node->prev = NULL;

    if (impl->head) {
        impl->head->prev = node;
    } else {
        impl->tail = node;
    }
    impl->head = node;
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
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node =
        (cobalt_list_node_t *)list->alloc->alloc(list->alloc, sizeof(cobalt_list_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    node->data = item;
    node->prev = impl->tail;
    node->next = NULL;

    if (impl->tail) {
        impl->tail->next = node;
    } else {
        impl->head = node;
    }
    impl->tail = node;
    impl->size++;
    return 0;
}

/**
 * @brief Remove and return the element at the head of the list
 */
void *cobalt_list_pop_front(cobalt_list_t *list)
{
    if (!list || !list->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node = impl->head;

    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    void *data = node->data;
    list->alloc->free(list->alloc, node);
    impl->size--;
    return data;
}

/**
 * @brief Remove and return the element at the tail of the list
 */
void *cobalt_list_pop_back(cobalt_list_t *list)
{
    if (!list || !list->tail) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node = impl->tail;

    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    void *data = node->data;
    list->alloc->free(list->alloc, node);
    impl->size--;
    return data;
}

/**
 * @brief Get the element at the specified index
 */
void *cobalt_list_get(const cobalt_list_t *list, size_t index)
{
    if (!list) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    const cobalt_list_t *impl = list;
    if (index >= impl->size) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_BOUNDS);
        return NULL;
    }

    cobalt_list_node_t *node;
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
size_t cobalt_list_size(const cobalt_list_t *list)
{
    return list ? list->size : 0;
}

/**
 * @brief Check if the list is empty
 */
int cobalt_list_is_empty(const cobalt_list_t *list)
{
    return list && list->size == 0;
}

/* ========================================================================= */
/* List Iterator implementation                                                */
/* ========================================================================= */

/**
 * @brief Iterator internal context structure, saves iteration progress
 */
typedef struct {
    cobalt_list_t      *list;    /**< The iterated list itself (currently unused) */
    cobalt_list_node_t *current; /**< Node currently pointed to */
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
    impl->current = impl->current->next;
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

    cobalt_list_t        *impl      = list;
    list_iterator_impl_t *iter_data = (list_iterator_impl_t *)cobalt_allocator_get_system()->alloc(
        cobalt_allocator_get_system(), sizeof(list_iterator_impl_t));
    if (!iter_data) {
        return NULL;
    }

    // Initialize iterator progress
    iter_data->list    = impl;
    iter_data->current = impl->head;

    // Create common iterator structure and bind method table
    cobalt_iterator_t *iter = (cobalt_iterator_t *)cobalt_allocator_get_system()->alloc(
        cobalt_allocator_get_system(), sizeof(cobalt_iterator_t));
    if (!iter) {
        free(iter_data);
        return NULL;
    }

    iter->vtable = &list_vtable;
    iter->data   = iter_data;
    return iter;
}

/**
 * @brief Remove the first occurrence of an element from the list
 */
int cobalt_list_remove(cobalt_list_t *list, void *item)
{
    if (!list || !item) {
        return -1;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node = impl->head;
    cobalt_list_node_t *prev = NULL;

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
            list->alloc->free(list->alloc, node);
            impl->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

/**
 * @brief Remove the first element for which the predicate returns non-zero
 */
int cobalt_list_remove_if(cobalt_list_t *list,
                          int (*predicate)(const void *item, void *user_data),
                          void *user_data)
{
    if (!list || !predicate) {
        return -1;
    }
    cobalt_list_t      *impl = list;
    cobalt_list_node_t *node = impl->head;
    cobalt_list_node_t *prev = NULL;

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
            list->alloc->free(list->alloc, node);
            impl->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

/* ========================================================================= */
/* Merge sort for linked list                                                */
/* ========================================================================= */

/**
 * @brief Split a linked list into two halves (fast/slow pointer technique)
 * @param head Pointer to the head pointer of the list
 * @param front_ref Output: head of the first half
 * @param back_ref Output: head of the second half
 */
static void list_split(cobalt_list_node_t **head_ref,
                       cobalt_list_node_t **front_ref,
                       cobalt_list_node_t **back_ref)
{
    if (*head_ref == NULL) {
        *front_ref = *back_ref = NULL;
        return;
    }

    cobalt_list_node_t *slow = *head_ref;
    cobalt_list_node_t *fast = (*head_ref)->next;

    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }

    *front_ref = *head_ref;
    *back_ref  = slow->next;
    slow->next = NULL;
}

/**
 * @brief Merge two sorted linked lists by data value
 * @param a Head of first sorted list
 * @param b Head of second sorted list
 * @param compar Comparison function
 * @return Head of the merged sorted list
 */
static cobalt_list_node_t *
list_merge(cobalt_list_node_t *a, cobalt_list_node_t *b, compare_func_t compar)
{
    cobalt_list_node_t *result = NULL;

    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }

    if (compar(a->data, b->data) <= 0) {
        result       = a;
        result->next = list_merge(a->next, b, compar);
    } else {
        result       = b;
        result->next = list_merge(a, b->next, compar);
    }
    return result;
}

/**
 * @brief Recursive merge sort
 */
static cobalt_list_node_t *list_mergesort(cobalt_list_node_t *head, compare_func_t compar)
{
    if (!head || !head->next) {
        return head;
    }

    cobalt_list_node_t *a = NULL;
    cobalt_list_node_t *b = NULL;
    list_split(&head, &a, &b);
    a = list_mergesort(a, compar);
    b = list_mergesort(b, compar);
    return list_merge(a, b, compar);
}

/**
 * @brief Get the head node of the list
 */
cobalt_list_node_t *cobalt_list_get_head(const cobalt_list_t *list)
{
    return list ? list->head : NULL;
}

/**
 * @brief Set the head and tail nodes of the list
 */
void cobalt_list_set_head(cobalt_list_t *list, cobalt_list_node_t *head, cobalt_list_node_t *tail)
{
    if (list) {
        list->head = head;
        list->tail = tail;
    }
}

/**
 * @brief Merge sort for the linked list nodes (exposed for algorithm layer)
 */
void cobalt_list_merge_sort(cobalt_list_node_t **head_ref, size_t *count, compare_func_t compar)
{
    if (!head_ref || !compar) {
        return;
    }
    *head_ref = list_mergesort(*head_ref, compar);
    (void)count;
}
