#include "cobalt/container/list.h"
#include <stdlib.h>

/* Create a new list */
cobalt_list_t *cobalt_list_create(void) {
    cobalt_list_t *list = malloc(sizeof(cobalt_list_t));
    if (!list) return NULL;
    
    list->head = list->tail = NULL;
    list->size = 0;
    
    /* Note: In a fully implemented version with Sequence interface,
       we would initialize list->base.ops here */
    return list;
}

/* Destroy the list */
void cobalt_list_destroy(cobalt_list_t *list) {
    if (list) {
        /* Free all nodes in real implementation */
        free(list);
    }
}

/* Add to front */
int cobalt_list_push_front(cobalt_list_t *list, void *item) {
    if (!list) return -1;
    
    cobalt_list_node_t *node = malloc(sizeof(cobalt_list_node_t));
    if (!node) return -1;
    node->data = item;
    node->next = list->head;
    node->prev = NULL;
    if (list->head) list->head->prev = node;
    list->head = node;
    if (!list->tail) list->tail = node;
    list->size++;
    return 0;
}

/* Add to back */
int cobalt_list_push_back(cobalt_list_t *list, void *item) {
    if (!list) return -1;
    
    cobalt_list_node_t *node = malloc(sizeof(cobalt_list_node_t));
    if (!node) return -1;
    node->data = item;
    node->next = NULL;
    node->prev = list->tail;
    
    if (!list->tail) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size++;
    return 0;
}

/* Remove from front */
void *cobalt_list_pop_front(cobalt_list_t *list) {
    if (!list || !list->head) return NULL;
    
    cobalt_list_node_t *node = list->head;
    void *data = node->data;
    list->head = node->next;
    if (list->head) list->head->prev = NULL;
    else list->tail = NULL;
    free(node);
    list->size--;
    return data;
}

/* Remove from back - stub */
void *cobalt_list_pop_back(cobalt_list_t *list) {
    (void)list;
    return NULL;
}

/* Get element by index - stub (O(n) traversal needed) */
void *cobalt_list_get(cobalt_list_t *list, size_t index) {
    (void)list; (void)index;
    return NULL;
}

/* List size */
size_t cobalt_list_size(cobalt_list_t *list) {
    return list ? list->size : 0;
}

/* Is empty? */
int cobalt_list_is_empty(cobalt_list_t *list) {
    return list && list->size == 0;
}
