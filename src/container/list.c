#include "cobalt/container/list.h"
#include <stdlib.h>

/* Opaque node structure */
typedef struct list_node {
    void *data;
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

typedef struct {
    cobalt_sequence_t base;
    list_node_t *head;
    list_node_t *tail;
    size_t size;
} cobalt_list_impl_t;

/* Sequence operations */
static size_t list_size_seq(cobalt_sequence_t *self) {
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    return list->size;
}

static int list_is_empty_seq(cobalt_sequence_t *self) {
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    return list->size == 0;
}

static void list_add_seq(cobalt_sequence_t *self, void *item) {
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    list_node_t *node = malloc(sizeof(list_node_t));
    if (!node) return;
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
}

static void list_remove_seq(cobalt_sequence_t *self, void *item) {
    (void)self; (void)item;
}

static cobalt_iterator_t *list_iterator_seq(cobalt_sequence_t *self) {
    (void)self;
    return NULL;
}

cobalt_list_t *cobalt_list_create(void) {
    cobalt_list_impl_t *list = malloc(sizeof(cobalt_list_impl_t));
    if (!list) return NULL;
    
    list->head = list->tail = NULL;
    list->size = 0;
    
    list->base.size = list_size_seq;
    list->base.is_empty = list_is_empty_seq;
    list->base.add = list_add_seq;
    list->base.remove = list_remove_seq;
    list->base.iterator = list_iterator_seq;
    
    return (cobalt_list_t *)list;
}

void cobalt_list_destroy(cobalt_list_t *list) {
    if (!list) return;
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t *node = impl->head;
    while (node) {
        list_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(list);
}

int cobalt_list_push_front(cobalt_list_t *list, void *item) {
    if (!list) return -1;
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t *node = malloc(sizeof(list_node_t));
    if (!node) return -1;
    node->data = item;
    node->next = impl->head;
    node->prev = NULL;
    if (impl->head) impl->head->prev = node;
    impl->head = node;
    if (!impl->tail) impl->tail = node;
    impl->size++;
    return 0;
}

int cobalt_list_push_back(cobalt_list_t *list, void *item) {
    if (!list) return -1;
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t *node = malloc(sizeof(list_node_t));
    if (!node) return -1;
    node->data = item;
    node->next = NULL;
    node->prev = impl->tail;
    
    if (!impl->tail) {
        impl->head = impl->tail = node;
    } else {
        impl->tail->next = node;
        impl->tail = node;
    }
    impl->size++;
    return 0;
}

void *cobalt_list_pop_front(cobalt_list_t *list) {
    if (!list || !((cobalt_list_impl_t *)list)->head) return NULL;
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t *node = impl->head;
    void *data = node->data;
    impl->head = node->next;
    if (impl->head) impl->head->prev = NULL;
    else impl->tail = NULL;
    free(node);
    impl->size--;
    return data;
}

void *cobalt_list_pop_back(cobalt_list_t *list) {
    (void)list;
    return NULL;
}

void *cobalt_list_get(cobalt_list_t *list, size_t index) {
    (void)list; (void)index;
    return NULL;
}

size_t cobalt_list_size(cobalt_list_t *list) {
    return list ? ((cobalt_list_impl_t *)list)->size : 0;
}

int cobalt_list_is_empty(cobalt_list_t *list) {
    return list && ((cobalt_list_impl_t *)list)->size == 0;
}
