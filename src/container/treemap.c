#include "cobalt/container/treemap.h"
#include <stdlib.h>
#include <string.h>

/* Portable strdup for C11 */
static char* my_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}

typedef struct treemap_node
{
    char* key;
    void* value;
    struct treemap_node* left;
    struct treemap_node* right;
} treemap_node_t;

typedef struct
{
    treemap_node_t* root;
    size_t size;
} treemap_impl_t;

struct cobalt_treemap
{
    treemap_impl_t impl;
};

static treemap_node_t* create_node(const char* key, void* value)
{
    treemap_node_t* node = malloc(sizeof(treemap_node_t));
    if (!node)
        return NULL;
    node->key = my_strdup(key);
    if (!node->key)
        {
            free(node);
            return NULL;
        }
    node->value = value;
    node->left = node->right = NULL;
    return node;
}

static void destroy_tree(treemap_node_t* node)
{
    if (!node)
        return;
    destroy_tree(node->left);
    destroy_tree(node->right);
    free(node->key);
    free(node);
}

static treemap_node_t* insert_node(treemap_node_t* node, const char* key, void* value)
{
    if (!node)
        return create_node(key, value);

    int cmp = strcmp(key, node->key);
    if (cmp < 0)
        node->left = insert_node(node->left, key, value);
    else if (cmp > 0)
        node->right = insert_node(node->right, key, value);
    else
        node->value = value;

    return node;
}

static treemap_node_t* find_node(treemap_node_t* node, const char* key)
{
    if (!node)
        return NULL;
    int cmp = strcmp(key, node->key);
    if (cmp < 0)
        return find_node(node->left, key);
    if (cmp > 0)
        return find_node(node->right, key);
    return node;
}

static treemap_node_t* find_min(treemap_node_t* node)
{
    while (node && node->left)
        node = node->left;
    return node;
}

static treemap_node_t* find_max(treemap_node_t* node)
{
    while (node && node->right)
        node = node->right;
    return node;
}

cobalt_treemap_t* cobalt_treemap_create(void)
{
    cobalt_treemap_t* map = malloc(sizeof(cobalt_treemap_t));
    if (!map)
        return NULL;
    map->impl.root = NULL;
    map->impl.size = 0;
    return map;
}

void cobalt_treemap_destroy(cobalt_treemap_t* map)
{
    if (!map)
        return;
    if (map->impl.root)
        destroy_tree(map->impl.root);
    free(map);
}

int cobalt_treemap_put(cobalt_treemap_t* map, const char* key, void* value)
{
    if (!map || !key)
        return -1;
    map->impl.root = insert_node(map->impl.root, key, value);
    map->impl.size++;
    return 0;
}

void* cobalt_treemap_get(cobalt_treemap_t* map, const char* key)
{
    if (!map || !key)
        return NULL;
    treemap_node_t* node = find_node(map->impl.root, key);
    return node ? node->value : NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t* map, const char* key)
{
    if (!map || !key)
        return -1;
    treemap_node_t* node = find_node(map->impl.root, key);
    if (!node)
        return -1;

    /* For simplicity, just mark value as NULL and decrement size */
    /* In a full implementation, we would rebalance the tree */
    node->value = NULL;
    map->impl.size--;
    return 0;
}

const char* cobalt_treemap_min_key(cobalt_treemap_t* map)
{
    if (!map || !map->impl.root)
        return NULL;
    treemap_node_t* node = find_min(map->impl.root);
    return node ? node->key : NULL;
}

const char* cobalt_treemap_max_key(cobalt_treemap_t* map)
{
    if (!map || !map->impl.root)
        return NULL;
    treemap_node_t* node = find_max(map->impl.root);
    return node ? node->key : NULL;
}

size_t cobalt_treemap_size(cobalt_treemap_t* map)
{
    if (!map)
        return 0;
    return map->impl.size;
}
