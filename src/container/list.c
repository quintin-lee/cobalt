#include "cobalt/container/list.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/* Opaque node structure */
typedef struct list_node
{
    void* data;
    struct list_node* next;
    struct list_node* prev;
} list_node_t;

typedef struct
{
    cobalt_sequence_t base;
    list_node_t* head;
    list_node_t* tail;
    size_t size;
} cobalt_list_impl_t;

/* Sequence operations */
static size_t list_size_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return list->size;
}

static int list_is_empty_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return list->size == 0;
}

static void list_add_seq(cobalt_sequence_t* self, void* item)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    list_node_t* node = malloc(sizeof(list_node_t));
    if (!node)
        return;
    node->data = item;
    node->next = NULL;
    node->prev = list->tail;

    if (!list->tail)
        {
            list->head = list->tail = node;
        }
    else
        {
            list->tail->next = node;
            list->tail = node;
        }
    list->size++;
}

static void list_remove_seq(cobalt_sequence_t* self, void* item)
{
    (void)self;
    (void)item;
}

static cobalt_iterator_t* list_iterator_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return cobalt_list_iterator_create((cobalt_list_t*)list);
}

cobalt_list_t* cobalt_list_create(void)
{
    cobalt_list_impl_t* list = malloc(sizeof(cobalt_list_impl_t));
    if (!list)
        return NULL;

    list->head = list->tail = NULL;
    list->size = 0;

    list->base.size = list_size_seq;
    list->base.is_empty = list_is_empty_seq;
    list->base.add = list_add_seq;
    list->base.remove = list_remove_seq;
    list->base.iterator = list_iterator_seq;

    return (cobalt_list_t*)list;
}

void cobalt_list_destroy(cobalt_list_t* list)
{
    if (!list)
        return;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = impl->head;
    while (node)
        {
            list_node_t* next = node->next;
            free(node);
            node = next;
        }
    free(list);
}

int cobalt_list_push_front(cobalt_list_t* list, void* item)
{
    if (!list)
        return -1;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = malloc(sizeof(list_node_t));
    if (!node)
        {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return -1;
        }
    node->data = item;
    node->next = impl->head;
    node->prev = NULL;
    if (impl->head)
        impl->head->prev = node;
    impl->head = node;
    if (!impl->tail)
        impl->tail = node;
    impl->size++;
    return 0;
}

int cobalt_list_push_back(cobalt_list_t* list, void* item)
{
    if (!list)
        return -1;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = malloc(sizeof(list_node_t));
    if (!node)
        {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return -1;
        }
    node->data = item;
    node->next = NULL;
    node->prev = impl->tail;

    if (!impl->tail)
        {
            impl->head = impl->tail = node;
        }
    else
        {
            impl->tail->next = node;
            impl->tail = node;
        }
    impl->size++;
    return 0;
}

void* cobalt_list_pop_front(cobalt_list_t* list)
{
    if (!list || !((cobalt_list_impl_t*)list)->head)
        return NULL;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = impl->head;
    void* data = node->data;
    impl->head = node->next;
    if (impl->head)
        impl->head->prev = NULL;
    else
        impl->tail = NULL;
    free(node);
    impl->size--;
    return data;
}

void* cobalt_list_pop_back(cobalt_list_t* list)
{
    if (!list || !((cobalt_list_impl_t*)list)->tail)
        return NULL;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = impl->tail;
    void* data = node->data;
    impl->tail = node->prev;
    if (impl->tail)
        impl->tail->next = NULL;
    else
        impl->head = NULL;
    free(node);
    impl->size--;
    return data;
}

void* cobalt_list_get(cobalt_list_t* list, size_t index)
{
    if (!list)
        return NULL;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    if (index >= impl->size)
        return NULL;

    list_node_t* node;
    if (index < impl->size / 2)
    {
        node = impl->head;
        for (size_t i = 0; i < index; i++)
            node = node->next;
    }
    else
    {
        node = impl->tail;
        for (size_t i = impl->size - 1; i > index; i--)
            node = node->prev;
    }
    return node->data;
}

size_t cobalt_list_size(cobalt_list_t* list)
{
    return list ? ((cobalt_list_impl_t*)list)->size : 0;
}

int cobalt_list_is_empty(cobalt_list_t* list)
{
    return list && ((cobalt_list_impl_t*)list)->size == 0;
}

/* List iterator - uses function pointer overrides */
typedef struct
{
    /* Context */
    cobalt_list_impl_t* list;
    list_node_t* current;
} list_iterator_impl_t;

static int list_iterator_has_next(void* ctx)
{
    list_iterator_impl_t* impl = (list_iterator_impl_t*)ctx;
    return impl->current != NULL;
}

static void* list_iterator_next(void* ctx)
{
    list_iterator_impl_t* impl = (list_iterator_impl_t*)ctx;
    if (!impl->current)
        return NULL;

    void* data = impl->current->data;
    impl->current = impl->current->next;
    return data;
}

static void list_iterator_destroy(void* ctx)
{
    if (ctx)
        free(ctx);
}

static const cobalt_iterator_vtable_t list_vtable = {
    .has_next = list_iterator_has_next,
    .next = list_iterator_next,
    .destroy = list_iterator_destroy
};

cobalt_iterator_t* cobalt_list_iterator_create(cobalt_list_t* list)
{
    if (!list)
        return NULL;

    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_iterator_impl_t* iter_data = malloc(sizeof(list_iterator_impl_t));
    if (!iter_data)
        return NULL;

    iter_data->list = impl;
    iter_data->current = impl->head;

    cobalt_iterator_t* iter = malloc(sizeof(cobalt_iterator_t));
    if (!iter)
        {
            free(iter_data);
            return NULL;
        }

    iter->vtable = &list_vtable;
    iter->data = iter_data;
    return iter;
}
