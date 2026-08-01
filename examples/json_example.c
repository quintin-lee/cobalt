/**
 * @file json_example.c
 * @brief Demonstrates JSON parsing and serialization
 *
 * Shows:
 * - Parsing JSON text into a tree structure
 * - Navigating and extracting values
 * - Serializing back to JSON string
 * - Proper cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cobalt/cobalt.h>

int main(void) {
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Sample JSON string */
    const char *json_text = "{\"name\": \"Alice\", \"age\": 30, \"city\": \"New York\", \"active\": true}";

    cobalt_info("Parsing JSON: %s\n", json_text);

    /* Parse into a tree */
    json_node_t *root = json_parse(json_text);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON\n");
        return 1;
    }

    cobalt_info("Parsed JSON tree with %d children\n", /* logic needed here */ 0);

    /* Extract values */
    json_node_t *name_node = json_tree_get_child(root, "name");
    json_node_t *age_node = json_tree_get_child(root, "age");
    json_node_t *city_node = json_tree_get_child(root, "city");
    json_node_t *active_node = json_tree_get_child(root, "active");

    const char *name = json_get_string(name_node);
    int age = (int)json_get_number(age_node);
    const char *city = json_get_string(city_node);
    int active = json_is_object(active_node) ? 1 : 0; /* simplified */

    cobalt_info("Extracted: name=%s, age=%d, city=%s, active=%d\n",
                name, age, city, active);

    /* Serialize back to string */
    char *output = json_serialize(root);
    if (output) {
        cobalt_info("Serialized JSON: %s\n", output);
        free(output);
    }

    /* Clean up the tree */
    json_destroy(root);

    cobalt_info("JSON demo complete!\n");
    return 0;
}