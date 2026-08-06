/**
 * @file map_demo.c
 * @brief Demonstrates polymorphic map usage via the cobalt_map_t interface
 *
 * Shows:
 * - Creating HashMap, TreeMap, and Set through cobalt_map_t *
 * - Using convenience functions (cobalt_map_get/put/remove/contains/clear)
 * - Iterating with cobalt_map_iterator_t
 * - Casting back to concrete types for type-specific operations
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <string.h>

static void demonstrate_map(cobalt_map_t *map, const char *name)
{
    printf("\n=== %s (via cobalt_map_t) ===\n", name);

    /* Insert */
    cobalt_map_put(map, "apple",  6, (void *)0x1111);
    cobalt_map_put(map, "banana", 7, (void *)0x2222);
    cobalt_map_put(map, "cherry", 7, (void *)0x3333);
    printf("  Inserted 3 items, size = %zu\n", cobalt_map_size(map));

    /* Contains */
    printf("  contains('apple')  = %s\n",
           cobalt_map_contains(map, "apple",  6)  ? "true" : "false");
    printf("  contains('grape')  = %s\n",
           cobalt_map_contains(map, "grape",  6)  ? "true" : "false");

    /* Get */
    void *val = cobalt_map_get(map, "banana", 7);
    printf("  get('banana') = %p\n", val);

    /* Remove */
    cobalt_map_remove(map, "cherry", 7);
    printf("  After remove('cherry'), size = %zu\n", cobalt_map_size(map));

    /* Iterate */
    printf("  Iterating:");
    cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
    if (iter) {
        while (cobalt_map_iterator_has_next(iter)) {
            cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
            printf(" %s", (const char *)pair.key);
        }
        cobalt_map_iterator_destroy(iter);
    }
    printf("\n");

    /* Clear */
    cobalt_map_clear(map);
    printf("  After clear, size = %zu, empty = %s\n",
           cobalt_map_size(map), cobalt_map_is_empty(map) ? "true" : "false");
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Polymorphic: use same code for all three types */
    cobalt_hashmap_t *hm = cobalt_hashmap_create(8);
    cobalt_treemap_t *tm = cobalt_treemap_create();
    cobalt_set_t     *st = cobalt_set_create(8);
    if (!hm || !tm || !st) {
        fprintf(stderr, "Failed to create maps\n");
        return 1;
    }

    demonstrate_map((cobalt_map_t *)hm, "HashMap");
    demonstrate_map((cobalt_map_t *)tm, "TreeMap");
    demonstrate_map((cobalt_map_t *)st, "Set");

    /* Type-specific: TreeMap ordering via concrete API */
    printf("\n=== TreeMap ordering (concrete API) ===\n");
    tm = cobalt_treemap_create();
    cobalt_treemap_put(tm, "cherry", (void *)1);
    cobalt_treemap_put(tm, "apple",  (void *)2);
    cobalt_treemap_put(tm, "banana", (void *)3);
    printf("  min_key = %s\n", cobalt_treemap_min_key(tm));
    printf("  max_key = %s\n", cobalt_treemap_max_key(tm));
    cobalt_treemap_destroy(tm);

    /* Type-specific: HashMap iterator factory */
    printf("\n=== HashMap iterator factory ===\n");
    hm = cobalt_hashmap_create(8);
    cobalt_hashmap_put(hm, "x", (void *)10);
    cobalt_hashmap_put(hm, "y", (void *)20);
    cobalt_map_iterator_t *iter = cobalt_hashmap_iterator_create(hm);
    if (iter) {
        while (cobalt_map_iterator_has_next(iter)) {
            cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
            printf("  key=%s value=%p\n", (const char *)pair.key, pair.value);
        }
        cobalt_map_iterator_destroy(iter);
    }
    cobalt_hashmap_destroy(hm);

    /* Type-specific: Set iterator factory */
    printf("\n=== Set iterator factory ===\n");
    st = cobalt_set_create(8);
    cobalt_set_insert(st, "alpha");
    cobalt_set_insert(st, "beta");
    cobalt_map_iterator_t *iter2 = cobalt_set_iterator_create(st);
    if (iter2) {
        while (cobalt_map_iterator_has_next(iter2)) {
            cobalt_map_pair_t pair = cobalt_map_iterator_next(iter2);
            printf("  key=%s value=%p\n", (const char *)pair.key, pair.value);
        }
        cobalt_map_iterator_destroy(iter2);
    }
    cobalt_set_destroy(st);

    cobalt_info("Map demo complete!\n");
    return 0;
}
