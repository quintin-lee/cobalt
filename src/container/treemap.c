/**
 * @file treemap.c
 * @brief Tree map (TreeMap) container implementation
 * @details Simplified implementation version, currently uses an ordinary binary search tree (BST) instead of a Red-Black tree.
 * Provides basic operations such as insertion, lookup, and extremum query, deletion operation only uses a soft delete marker.
 */

#include "cobalt/container/treemap.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Binary search tree node structure
 */
typedef struct treemap_node {
    char                *key;   /* Copy of the string key */
    void                *value; /* Stored value */
    struct treemap_node *left;  /* Left child node */
    struct treemap_node *right; /* Right child node */
} treemap_node_t;

/**
 * @brief Internal implementation structure of TreeMap
 */
typedef struct {
    treemap_node_t *root; /* Root node of the binary tree */
    size_t          size; /* Number of valid elements */
} treemap_impl_t;

/**
 * @brief TreeMap structure encapsulation
 */
struct cobalt_treemap {
    treemap_impl_t impl;
};

/**
 * @brief Create a new tree node
 * 
 * @param key String key
 * @param value Value pointer
 * @return Returns the newly created node, NULL if out of memory
 */
static treemap_node_t *create_node(const char *key, void *value)
{
    treemap_node_t *node = malloc(sizeof(treemap_node_t));
    if (!node) {
        return NULL;
    }
    // Deep copy key
    node->key = cobalt_strdup(key);
    if (!node->key) {
        free(node);
        return NULL;
    }
    node->value = value;
    node->left = node->right = NULL;
    return node;
}

/**
 * @brief Recursively destroy the binary tree
 * @details Uses post-order traversal to free all nodes and key copies.
 * 
 * @param node Current tree node to destroy
 */
static void destroy_tree(treemap_node_t *node)
{
    if (!node) {
        return;
    }
    destroy_tree(node->left);
    destroy_tree(node->right);
    free(node->key);
    free(node);
}

/**
 * @brief Recursively insert or update a node in the tree
 * @details Decides whether to insert into the left or right subtree based on string comparison results. If the key already exists, updates the value.
 * 
 * @param node Current tree node
 * @param key String key
 * @param value Value to store
 * @return Returns the root node of the subtree after insertion or update
 */
static treemap_node_t *insert_node(treemap_node_t *node, const char *key, void *value)
{
    // Find the insertion position and create a new node
    if (!node) {
        return create_node(key, value);
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = insert_node(node->left, key, value);
    } else if (cmp > 0) {
        node->right = insert_node(node->right, key, value);
    } else {
        // Key already exists, update value
        node->value = value;
    }

    return node;
}

/**
 * @brief Recursively find the specified node
 * 
 * @param node Current tree node
 * @param key Key to look for
 * @return Returns the found node, NULL if not found
 */
static treemap_node_t *find_node(treemap_node_t *node, const char *key)
{
    if (!node) {
        return NULL;
    }
    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        return find_node(node->left, key);
    }
    if (cmp > 0) {
        return find_node(node->right, key);
    }
    return node;
}

/**
 * @brief Find the node with the minimum key in the tree
 * @details Traverses down the left subtree until a leaf node is reached.
 * 
 * @param node Root node of the tree
 * @return Returns the minimum node
 */
static treemap_node_t *find_min(treemap_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

/**
 * @brief Find the node with the maximum key in the tree
 * @details Traverses down the right subtree until a leaf node is reached.
 * 
 * @param node Root node of the tree
 * @return Returns the maximum node
 */
static treemap_node_t *find_max(treemap_node_t *node)
{
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

/**
 * @brief Create an empty TreeMap
 * 
 * @return Returns TreeMap pointer, NULL if out of memory
 */
cobalt_treemap_t *cobalt_treemap_create(void)
{
    cobalt_treemap_t *map = malloc(sizeof(cobalt_treemap_t));
    if (!map) {
        return NULL;
    }
    map->impl.root = NULL;
    map->impl.size = 0;
    return map;
}

/**
 * @brief Destroy TreeMap
 * 
 * @param map TreeMap pointer
 */
void cobalt_treemap_destroy(cobalt_treemap_t *map)
{
    if (!map) {
        return;
    }
    if (map->impl.root) {
        destroy_tree(map->impl.root);
    }
    free(map);
}

/**
 * @brief Insert or update a key-value pair in TreeMap
 * 
 * @param map TreeMap pointer
 * @param key String key
 * @param value Value to store
 * @return Returns 0 on success, -1 on parameter error
 */
int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        return -1;
    }
    map->impl.root = insert_node(map->impl.root, key, value);
    map->impl.size++;
    return 0;
}

/**
 * @brief Get the value corresponding to the specified key
 * 
 * @param map TreeMap pointer
 * @param key String key
 * @return Returns the found value, NULL if not found
 */
void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return NULL;
    }
    treemap_node_t *node = find_node(map->impl.root, key);
    return node ? node->value : NULL;
}

/**
 * @brief Remove the specified key-value pair
 * @details Current implementation uses a simplified "soft delete", setting the value of the corresponding node to NULL and decrementing size, without performing physical deletion and tree structure rebalancing.
 * 
 * @param map TreeMap pointer
 * @param key String key
 * @return Returns 0 on successful removal, -1 if not found or parameter error
 */
int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *node = find_node(map->impl.root, key);
    if (!node) {
        return -1;
    }

    /* To simplify implementation, just mark value as NULL and decrease size.
       In a complete implementation, we should perform actual node deletion and tree rebalancing operations. */
    node->value = NULL;
    map->impl.size--;
    return 0;
}

/**
 * @brief Get the minimum key in the map
 * 
 * @param map TreeMap pointer
 * @return Minimum string key, NULL if empty tree
 */
const char *cobalt_treemap_min_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = find_min(map->impl.root);
    return node ? node->key : NULL;
}

/**
 * @brief Get the maximum key in the map
 * 
 * @param map TreeMap pointer
 * @return Maximum string key, NULL if empty tree
 */
const char *cobalt_treemap_max_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = find_max(map->impl.root);
    return node ? node->key : NULL;
}

/**
 * @brief Get the number of valid elements
 * 
 * @param map TreeMap pointer
 * @return Number of valid key-value pairs
 */
size_t cobalt_treemap_size(cobalt_treemap_t *map)
{
    if (!map) {
        return 0;
    }
    return map->impl.size;
}
