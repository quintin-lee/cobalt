/**
 * @file json_example.c
 * @brief Demonstrates JSON parsing and serialization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cobalt/cobalt.h>

int main(void) {
    /* Sample JSON string */
    const char *json_text = "{\"name\": \"Alice\", \"age\": 30}";

    printf("=== JSON Example ===\n\n");
    printf("Parsing JSON: %s\n", json_text);

    /* Parse into a tree */
    json_node_t *root = json_parse(json_text);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON\n");
        return 1;
    }

    /* Extract values */
    json_node_t *name_node = json_tree_get_child(root, "name");
    json_node_t *age_node = json_tree_get_child(root, "age");

    const char *name = json_get_string(name_node);
    double age = json_get_number(age_node);

    printf("Extracted: name=%s, age=%.0f\n", name, age);

    /* Serialize back to string */
    char *output = json_serialize(root);
    if (output) {
        printf("Serialized: %s\n", output);
        free(output);
    }

    /* Clean up */
    json_destroy(root);

    printf("\n=== Example completed ===\n");
    return 0;
}
