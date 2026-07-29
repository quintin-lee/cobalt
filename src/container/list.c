#include "container/list.h"
#include <stdlib.h>

static size_t list_size(cobalt_sequence_t *self) {
  cobalt_list_t *list = (cobalt_list_t *)self;
  return list->size;
}

static int list_is_empty(cobalt_sequence_t *self) {
  cobalt_list_t *list = (cobalt_list_t *)self;
  return list->size == 0;
}

static void list_add(cobalt_sequence_t *self, void *item) {
  cobalt_list_t *list = (cobalt_list_t *)self;
  cobalt_list_node_t *node = malloc(sizeof(cobalt_list_node_t));
  if (!node) return;
  node->data = item;
  node->next = NULL;
  node->prev = list->tail;
  
  if (!list->head) {
    list->head = list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
}

static void list_remove(cobalt_sequence_t *self, void *item) {
  (void)self; (void)item;
}

static cobalt_iterator_t *list_iterator(cobalt_sequence_t *self) {
  (void)self;
  return NULL;
}

static const cobalt_sequence_ops list_ops = {
  .size = list_size,
  .is_empty = list_is_empty,
  .add = list_add,
  .remove = list_remove,
  .iterator = list_iterator
};

cobalt_list_t *cobalt_list_create(void) {
  cobalt_list_t *list = malloc(sizeof(cobalt_list_t));
  if (!list) return NULL;
  list->base.ops = &list_ops;
  list->head = list->tail = NULL;
  list->size = 0;
  return list;
}

void cobalt_list_destroy(cobalt_list_t *list) {
  if (list) free(list);
}

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

int cobalt_list_push_back(cobalt_list_t *list, void *item) {
  list_add((cobalt_sequence_t *)list, item);
  return 0;
}

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

void *cobalt_list_pop_back(cobalt_list_t *list) {
  (void)list;
  return NULL;
}

void *cobalt_list_get(cobalt_list_t *list, size_t index) {
  (void)list; (void)index;
  return NULL;
}

size_t cobalt_list_size(cobalt_list_t *list) {
  return list ? list->size : 0;
}

int cobalt_list_is_empty(cobalt_list_t *list) {
  return list && list->size == 0;
}
