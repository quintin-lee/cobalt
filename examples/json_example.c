/**
 * @file json_example.c
 * @brief Demonstrates JSON parsing
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char* json_text = "{\"name\": \"Alice\", \"age\": 30}";

    printf("=== JSON Example ===\n\n");
    printf("Parsing JSON: %s\n", json_text);

    json_node_t* root = json_parse(json_text);
    if (!root)
        {
            fprintf(stderr, "Failed to parse JSON\n");
            return 1;
        }
    printf("Parsed OK\n");

    json_node_t* name_node = json_tree_get_child(root, "name");
    json_node_t* age_node = json_tree_get_child(root, "age");

    if (name_node)
        {
            printf("name: %s\n", json_get_string(name_node));
        }
    if (age_node)
        {
            printf("age: %.0f\n", json_get_number(age_node));
        }

    json_destroy(root);
    printf("\n=== Example completed ===\n");
    return 0;
}
