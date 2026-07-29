#include "cobalt/container/treemap.h"
#include <stdlib.h>
#include <string.h>

/* Internal BST node (unbalanced - placeholder for red-black) */
typedef struct treemap_node {
    char *key;
    void *value;
    struct treemap_node *left;
    struct treemap_node *right;
} treemap_node_t;

/* Internal implementation structure */
typedef struct {
    treemap_node_t *root;
    size_t size;
} treemap_impl_t;

/* Helper: create a new node */
static treemap_node_t* create_node(const char *key, void *value) {
    treemap_node_t *node = malloc(sizeof(treemap_node_t));
    if (node) {
        node->key = strdup(key);
        node->value = value;
        node->left = node->right = NULL;
    }
    return node;
}

/* Simple BST insert (not balanced) */
static treemap_node_t* insert(treemap_node_t **root, const char *key, void *value) {
    if (*root == NULL) {
        return create_node(key, value);
    }
    int cmp = strcmp(key, (*root)->key);
    if (cmp < 0) {
        (*root)->left = insert(&(*root)->left, key, value);
    } else if (cmp > 0) {
        (*root)->right = insert(&(*root)->right, key, value);
    } else {
        (*root)->value = value;
    }
    return *root;
}

/* Simple BST search */
static treemap_node_t* search(treemap_node_t *root, const char *key) {
    if (!root || strcmp(root->key, key) == 0) {
        return root;
    }
    int cmp = strcmp(key, root->key);
    if (cmp < 0) return search(root->left, key);
    return search(root->right, key);
}

/* Find minimum key (leftmost) */
static const char* find_min(treemap_node_t *node) {
    while (node && node->left) node = node->left;
    return node ? node->key : NULL;
}

/* Find maximum key (rightmost) */
static const char* find_max(treemap_node_t *node) {
    while (node && node->right) node = node->right;
    return node ? node->key : NULL;
}

/* Create a new treemap */
cobalt_treemap_t cobalt_treemap_create(void) {
    treemap_impl_t *impl = malloc(sizeof(treemap_impl_t));
    if (!impl) return NULL;
    impl->root = NULL;
    impl->size = 0;
    return (cobalt_treemap_t)impl;
}

/* Destroy the treemap */
void cobalt_treemap_destroy(cobalt_treemap_t map) {
    if (!map) return;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    free(impl);
}

/* Set value for key */
int cobalt_treemap_put(cobalt_treemap_t map, const char *key, void *value) {
    if (!map || !key) return -1;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    impl->root = insert(&(impl->root), key, value);
    impl->size++;
    return 0;
}

/* Get value by key */
void *cobalt_treemap_get(cobalt_treemap_t map, const char *key) {
    if (!map || !key) return NULL;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    treemap_node_t *node = search(impl->root, key);
    return node ? node->value : NULL;
}

/* Remove key from map (stub) */
int cobalt_treemap_remove(cobalt_treemap_t map, const char *key) {
    (void)map; (void)key;
    return 0;
}

/* Get minimum key */
const char *cobalt_treemap_min_key(cobalt_treemap_t map) {
    if (!map) return NULL;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    if (!impl || !impl->root) return NULL;
    return find_min(impl->root);
}

/* Get maximum key */
const char *cobalt_treemap_max_key(cobalt_treemap_t map) {
    if (!map) return NULL;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    if (!impl || !impl->root) return NULL;
    return find_max(impl->root);
}

/* Map size */
size_t cobalt_treemap_size(cobalt_treemap_t map) {
    if (!map) return 0;
    treemap_impl_t *impl = (treemap_impl_t *)map;
    return impl ? impl->size : 0;
}
