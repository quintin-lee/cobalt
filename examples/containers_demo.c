/**
 * @file containers_demo.c
 * @brief Demonstrate all container types
 */

#include <stdio.h>
#include "cobalt/cobalt.h"

int main(void) {
    printf("=== Containers Example ===\n\n");
    
    /* Vector */
    cobalt_vector_t *vec = cobalt_vector_create(5);
    int v1 = 1, v2 = 2, v3 = 3;
    cobalt_vector_push(vec, &v1);
    cobalt_vector_push(vec, &v2);
    cobalt_vector_push(vec, &v3);
    printf("Vector: size=%zu\n", cobalt_vector_size(vec));
    printf("  Front: %d, Back: %d\n", *(int*)cobalt_vector_get(vec, 0),
           *(int*)cobalt_vector_get(vec, 2));
    cobalt_vector_destroy(vec);
    
    /* List */
    cobalt_list_t *list = cobalt_list_create();
    cobalt_list_push_front(list, &v1);
    cobalt_list_push_back(list, &v2);
    printf("\nList: size=%zu\n", cobalt_list_size(list));
    printf("  Pop front: %d\n", *(int*)cobalt_list_pop_front(list));
    cobalt_list_destroy(list);
    
    /* HashMap */
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    int val1 = 10, val2 = 20;
    cobalt_hashmap_put(map, "a", &val1);
    cobalt_hashmap_put(map, "b", &val2);
    printf("\nHashMap: size=%zu\n", cobalt_hashmap_size(map));
    printf("  'a'=%d, 'b'=%d\n", *(int*)cobalt_hashmap_get(map, "a"),
           *(int*)cobalt_hashmap_get(map, "b"));
    cobalt_hashmap_destroy(map);
    
    /* TreeMap */
    cobalt_treemap_t *tree = cobalt_treemap_create();
    cobalt_treemap_put(tree, "banana", &v1);
    cobalt_treemap_put(tree, "apple", &v2);
    printf("\nTreeMap: size=%zu\n", cobalt_treemap_size(tree));
    printf("  Min: %s, Max: %s\n", cobalt_treemap_min_key(tree),
           cobalt_treemap_max_key(tree));
    cobalt_treemap_destroy(tree);
    
    /* Stack */
    cobalt_stack_t *stack = cobalt_stack_create();
    cobalt_stack_push(stack, &v1);
    cobalt_stack_push(stack, &v2);
    cobalt_stack_push(stack, &v3);
    printf("\nStack: size=%zu\n", cobalt_stack_size(stack));
    printf("  Pop: %d (LIFO)\n", *(int*)cobalt_stack_pop(stack));
    cobalt_stack_destroy(stack);
    
    printf("\n=== Example completed ===\n");
    return 0;
}
