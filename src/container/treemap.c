/**
 * @file treemap.c
 * @brief Binary Search Tree implementation for TreeMap container
 */

#include "cobalt/container/treemap.h"
#include "cobalt/platform/debug_assert.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

typedef struct treemap_node {
    char                *key;
    void                *value;
    struct treemap_node *left;
    struct treemap_node *right;
} treemap_node_t;

typedef struct {
    treemap_node_t *root;
    size_t          size;
} treemap_impl_t;

struct cobalt_treemap {
    treemap_impl_t impl;
};

static treemap_node_t *create_node(const char *key, void *value)
{
    treemap_node_t *node = malloc(sizeof(treemap_node_t));
    if (!node) {
        return NULL;
    }
    node->key = cobalt_strdup(key);
    if (!node->key) {
        free(node);
        return NULL;
    }
    node->value = value;
    node->left = node->right = NULL;
    return node;
}

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

static treemap_node_t *insert_node(treemap_node_t *node, const char *key, void *value)
{
    if (!node) {
        return create_node(key, value);
    }
    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = insert_node(node->left, key, value);
    } else if (cmp > 0) {
        node->right = insert_node(node->right, key, value);
    } else {
        node->value = value;
    }
    return node;
}

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

static treemap_node_t *find_min(treemap_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

static treemap_node_t *find_max(treemap_node_t *node)
{
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

static treemap_node_t *delete_min(treemap_node_t *node, size_t *size)
{
    if (!node || !node->left) {
        /* This is the minimum node */
        treemap_node_t *right = node->right;
        free(node->key);
        free(node);
        (*size)--;
        return right;
    }
    node->left = delete_min(node->left, size);
    return node;
}

static treemap_node_t *remove_node(treemap_node_t *node, const char *key, size_t *size)
{
    if (!node) {
        return NULL;
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = remove_node(node->left, key, size);
    } else if (cmp > 0) {
        node->right = remove_node(node->right, key, size);
    } else {
        if (!node->left && !node->right) {
            free(node->key);
            free(node);
            (*size)--;
            return NULL;
        } else if (!node->left) {
            treemap_node_t *tmp = node->right;
            free(node->key);
            free(node);
            (*size)--;
            return tmp;
        } else if (!node->right) {
            treemap_node_t *tmp = node->left;
            free(node->key);
            free(node);
            (*size)--;
            return tmp;
        } else {
            treemap_node_t *successor = find_min(node->right);
            char           *old_key   = node->key;
            node->key                 = cobalt_strdup(successor->key);
            node->value               = successor->value;
            free(old_key);
            node->right = delete_min(node->right, size);
        }
    }
    return node;
}

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

int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        return -1;
    }
    if (find_node(map->impl.root, key)) {
        map->impl.root = insert_node(map->impl.root, key, value);
    } else {
        map->impl.root = insert_node(map->impl.root, key, value);
        map->impl.size++;
    }
    return 0;
}

void *cobalt_treemap_get(const cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return NULL;
    }
    treemap_node_t *node = find_node(map->impl.root, key);
    return node ? node->value : NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    if (!find_node(map->impl.root, key)) {
        return -1;
    }
    map->impl.root = remove_node(map->impl.root, key, &map->impl.size);
    return 0;
}

const char *cobalt_treemap_min_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = find_min(map->impl.root);
    return node ? node->key : NULL;
}

const char *cobalt_treemap_max_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = find_max(map->impl.root);
    return node ? node->key : NULL;
}

size_t cobalt_treemap_size(const cobalt_treemap_t *map)
{
    return map ? map->impl.size : 0;
}
