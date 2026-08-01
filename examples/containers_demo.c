/**
 * @file containers_demo.c
 * @brief Demonstrate all container types
 */

#include "cobalt/cobalt.h"
#include <stdio.h>

int main(void)
{
    printf("=== Containers Example ===\n\n");

    /* Vector */
    cobalt_vector_t* vec = cobalt_vector_create(5);
    int v1 = 1, v2 = 2, v3 = 3;
    cobalt_vector_push(vec, &v1);
    cobalt_vector_push(vec, &v2);
    cobalt_vector_push(vec, &v3);
    printf("Vector: size=%zu\n", cobalt_vector_size(vec));
    cobalt_vector_destroy(vec);

    /* List */
    cobalt_list_t* list = cobalt_list_create();
    cobalt_list_push_front(list, &v1);
    cobalt_list_push_back(list, &v2);
    printf("List: size=%zu\n", cobalt_list_size(list));
    cobalt_list_destroy(list);

    /* HashMap */
    cobalt_hashmap_t* map = cobalt_hashmap_create(16);
    int val1 = 10, val2 = 20;
    cobalt_hashmap_put(map, "a", &val1);
    cobalt_hashmap_put(map, "b", &val2);
    printf("HashMap: size=%zu\n", cobalt_hashmap_size(map));
    cobalt_hashmap_destroy(map);

    /* TreeMap */
    cobalt_treemap_t* tree = cobalt_treemap_create();
    cobalt_treemap_put(tree, "banana", &v1);
    cobalt_treemap_put(tree, "apple", &v2);
    printf("TreeMap: size=%zu, min=%s, max=%s\n", cobalt_treemap_size(tree),
           cobalt_treemap_min_key(tree), cobalt_treemap_max_key(tree));
    cobalt_treemap_destroy(tree);

    /* Stack */
    cobalt_stack_t* stack = cobalt_stack_create();
    cobalt_stack_push(stack, &v1);
    cobalt_stack_push(stack, &v2);
    cobalt_stack_push(stack, &v3);
    printf("Stack: size=%zu\n", cobalt_stack_size(stack));
    cobalt_stack_destroy(stack);

    /* Queue */
    cobalt_queue_t* queue = cobalt_queue_create();
    cobalt_queue_enqueue(queue, &v1);
    cobalt_queue_enqueue(queue, &v2);
    cobalt_queue_enqueue(queue, &v3);
    printf("Queue: size=%zu\n", cobalt_queue_size(queue));
    cobalt_queue_destroy(queue);

    printf("\n=== Example completed ===\n");
    return 0;
}
