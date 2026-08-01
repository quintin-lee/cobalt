/**
 * @file hashmap_demo.c
 * @brief Demonstrates using the HashMap container
 *
 * Shows:
 * - Creating a hash map with string keys
 * - Inserting key-value pairs
 * - Retrieving values by key
 * - Removing entries
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Create a hash map with initial bucket count of 16 */
    cobalt_hashmap_t* map = cobalt_hashmap_create(16);
    if (!map)
        {
            fprintf(stderr, "Failed to create hashmap\n");
            return 1;
        }

    /* Set some key-value pairs */
    const char* keys[] = {"name", "age", "city"};
    char* values[] = {"Alice", "30", "New York"};

    for (size_t i = 0; i < 3; i++)
        {
            int ret = cobalt_hashmap_put(map, keys[i], values[i]);
            if (ret == 0)
                {
                    cobalt_info("Put: %s -> %s\n", keys[i], values[i]);
                }
        }

    /* Retrieve values by key */
    char* name = (char*)cobalt_hashmap_get(map, "name");
    char* age = (char*)cobalt_hashmap_get(map, "age");
    if (name)
        cobalt_info("Name: %s\n", name);
    if (age)
        cobalt_info("Age: %s\n", age);

    /* Check size */
    cobalt_info("Map size: %zu\n", cobalt_hashmap_size(map));

    /* Remove an entry */
    cobalt_hashmap_remove(map, "city");
    cobalt_info("After remove, size: %zu\n", cobalt_hashmap_size(map));

    /* Destroy the map */
    cobalt_hashmap_destroy(map);

    cobalt_info("Hashmap demo complete!\n");
    return 0;
}