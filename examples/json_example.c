/**
 * @file json_example.c
 * @brief Demonstrates JSON parsing and the custom-allocator API variants
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *json_text = "{\"name\": \"Alice\", \"age\": 30}";

    printf("=== JSON Example ===\n\n");
    printf("Parsing JSON: %s\n", json_text);

    json_node_t *root = json_parse(json_text);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON\n");
        return 1;
    }
    printf("Parsed OK (system allocator)\n");

    json_node_t *name_node = json_tree_get_child(root, "name");
    json_node_t *age_node  = json_tree_get_child(root, "age");

    if (name_node) {
        printf("name: %s\n", json_get_string(name_node));
    }
    if (age_node) {
        printf("age: %.0f\n", json_get_number(age_node));
    }

    json_destroy(root);

    /* Demonstrate the custom-allocator API with the system allocator explicitly.
     * json_parse_with_alloc/json_serialize_with_alloc/json_destroy_with_alloc
     * accept NULL to fall back to the system allocator, or any injectable
     * cobalt_allocator_t* (e.g. from cobalt_arena_create + cobalt_allocator_wrap).
     */
    printf("\n--- Custom allocator path ---\n");
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();

    json_node_t *root2 = json_parse_with_alloc(json_text, alloc);
    if (!root2) {
        fprintf(stderr, "Failed to parse JSON (custom allocator)\n");
        return 1;
    }

    char *serialized = json_serialize_with_alloc(root2, alloc);
    if (serialized) {
        printf("Serialized: %s\n", serialized);
        alloc->free(alloc, serialized);
    }

    json_destroy_with_alloc(root2, alloc);

    printf("\n=== Example completed ===\n");
    return 0;
}
