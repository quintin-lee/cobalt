/**
 * @file treemap.c
 * @brief Red-Black Tree implementation for TreeMap container
 */

#include "cobalt/container/treemap.h"
#include "cobalt/platform/debug_assert.h"
#include "cobalt/utils/string.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

#define RB_RED   0
#define RB_BLACK 1

typedef struct treemap_node {
    char                *key;
    void                *value;
    struct treemap_node *left;
    struct treemap_node *right;
    struct treemap_node *parent;
    int                  color;
} treemap_node_t;

typedef struct {
    treemap_node_t *root;
    size_t          size;
} treemap_impl_t;

struct cobalt_treemap {
    treemap_impl_t impl;
};

/* ========================================================================= */
/* Node helpers                                                               */
/* ========================================================================= */

static treemap_node_t *rb_node_create(const char *key, void *value)
{
    treemap_node_t *node = malloc(sizeof(treemap_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    node->key       = cobalt_strdup(key);
    if (!node->key) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        free(node);
        return NULL;
    }
    node->value    = value;
    node->left     = node->right = node->parent = NULL;
    node->color    = RB_RED;
    return node;
}

static void rb_node_free(treemap_node_t *node)
{
    if (node) {
        free(node->key);
        free(node);
    }
}

static void rb_destroy_tree(treemap_node_t *node)
{
    if (!node) {
        return;
    }
    rb_destroy_tree(node->left);
    rb_destroy_tree(node->right);
    rb_node_free(node);
}

/* ========================================================================= */
/* Rotations — both take the full impl so they can update the root pointer   */
/* ========================================================================= */

static void rb_rotate_left(treemap_impl_t *tree, treemap_node_t *x)
{
    treemap_node_t *y = x->right;
    x->right = y->left;
    if (y->left) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (!x->parent) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

static void rb_rotate_right(treemap_impl_t *tree, treemap_node_t *x)
{
    treemap_node_t *y = x->left;
    x->left = y->right;
    if (y->right) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (!x->parent) {
        tree->root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
}

/* ========================================================================= */
/* Insert fixup                                                               */
/* ========================================================================= */

static void rb_insert_fixup(treemap_impl_t *tree, treemap_node_t *z)
{
    while (z->parent && z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            treemap_node_t *uncle = z->parent->parent->right;
            if (uncle && uncle->color == RB_RED) {
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rb_rotate_left(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_right(tree, z->parent->parent);
            }
        } else {
            treemap_node_t *uncle = z->parent->parent->left;
            if (uncle && uncle->color == RB_RED) {
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rb_rotate_right(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_left(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = RB_BLACK;
}

static void rb_insert(treemap_impl_t *tree, treemap_node_t *z)
{
    treemap_node_t *y = NULL;
    treemap_node_t *x = tree->root;

    while (x) {
        y = x;
        int cmp = strcmp(z->key, x->key);
        if (cmp < 0) {
            x = x->left;
        } else if (cmp > 0) {
            x = x->right;
        } else {
            /* Duplicate key — update value, discard new node */
            x->value = z->value;
            rb_node_free(z);
            return;
        }
    }
    z->parent = y;
    if (!y) {
        tree->root = z;
    } else if (strcmp(z->key, y->key) < 0) {
        y->left = z;
    } else {
        y->right = z;
    }
    rb_insert_fixup(tree, z);
    tree->size++;
}

/* ========================================================================= */
/* Delete fixup                                                               */
/* ========================================================================= */

static void rb_replace(treemap_impl_t *tree, treemap_node_t *old_node,
                       treemap_node_t *new_node)
{
    if (!old_node->parent) {
        tree->root = new_node;
    } else if (old_node == old_node->parent->left) {
        old_node->parent->left = new_node;
    } else {
        old_node->parent->right = new_node;
    }
    if (new_node) {
        new_node->parent = old_node->parent;
    }
}

static void rb_delete_fixup(treemap_impl_t *tree, treemap_node_t *x)
{
    while (x && x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            treemap_node_t *sibling = x->parent->right;
            if (sibling->color == RB_RED) {
                sibling->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_left(tree, x->parent);
                sibling = x->parent->right;
            }
            if ((!sibling->left || sibling->left->color == RB_BLACK) &&
                (!sibling->right || sibling->right->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x = x->parent;
            } else {
                if (!sibling->right || sibling->right->color == RB_BLACK) {
                    if (sibling->left) {
                        sibling->left->color = RB_BLACK;
                    }
                    sibling->color = RB_RED;
                    rb_rotate_right(tree, sibling);
                    sibling = x->parent->right;
                }
                sibling->color = x->parent->color;
                x->parent->color = RB_BLACK;
                if (sibling->right) {
                    sibling->right->color = RB_BLACK;
                }
                rb_rotate_left(tree, x->parent);
                x = tree->root;
            }
        } else {
            treemap_node_t *sibling = x->parent->left;
            if (sibling->color == RB_RED) {
                sibling->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_right(tree, x->parent);
                sibling = x->parent->left;
            }
            if ((!sibling->right || sibling->right->color == RB_BLACK) &&
                (!sibling->left || sibling->left->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x = x->parent;
            } else {
                if (!sibling->left || sibling->left->color == RB_BLACK) {
                    if (sibling->right) {
                        sibling->right->color = RB_BLACK;
                    }
                    sibling->color = RB_RED;
                    rb_rotate_left(tree, sibling);
                    sibling = x->parent->left;
                }
                sibling->color = x->parent->color;
                x->parent->color = RB_BLACK;
                if (sibling->left) {
                    sibling->left->color = RB_BLACK;
                }
                rb_rotate_right(tree, x->parent);
                x = tree->root;
            }
        }
    }
    if (x) {
        x->color = RB_BLACK;
    }
}

static void rb_delete(treemap_impl_t *tree, treemap_node_t *z)
{
    treemap_node_t *y = z;
    int y_original_color = y->color;
    treemap_node_t *x = NULL;

    if (!z->left) {
        x = z->right;
        rb_replace(tree, y, z->right);
    } else if (!z->right) {
        x = z->left;
        rb_replace(tree, y, z->left);
    } else {
        /* y is the in-order successor */
        y = z->right;
        while (y->left) {
            y = y->left;
        }
        y_original_color = y->color;
        x = y->right;
        if (y->parent != z) {
            rb_replace(tree, y, y->right);
            y->right = z->right;
            if (y->right) {
                y->right->parent = y;
            }
        }
        rb_replace(tree, z, y);
        y->left = z->left;
        if (y->left) {
            y->left->parent = y;
        }
        y->color = z->color;
    }

    rb_node_free(z);
    tree->size--;

    if (y_original_color == RB_BLACK) {
        rb_delete_fixup(tree, x);
    }
}

/* ========================================================================= */
/* Find helpers                                                               */
/* ========================================================================= */

static treemap_node_t *rb_find(treemap_node_t *node, const char *key)
{
    while (node) {
        int cmp = strcmp(key, node->key);
        if (cmp < 0) {
            node = node->left;
        } else if (cmp > 0) {
            node = node->right;
        } else {
            return node;
        }
    }
    return NULL;
}

static treemap_node_t *rb_find_min(treemap_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

static treemap_node_t *rb_find_max(treemap_node_t *node)
{
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

cobalt_treemap_t *cobalt_treemap_create(void)
{
    cobalt_treemap_t *map = malloc(sizeof(cobalt_treemap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
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
        rb_destroy_tree(map->impl.root);
    }
    free(map);
}

int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *existing = rb_find(map->impl.root, key);
    if (existing) {
        existing->value = value;
        return 0;
    }
    treemap_node_t *node = rb_node_create(key, value);
    if (!node) {
        return -1;
    }
    rb_insert(&map->impl, node);
    return 0;
}

void *cobalt_treemap_get(const cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return NULL;
    }
    treemap_node_t *node = rb_find(map->impl.root, key);
    return node ? node->value : NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *node = rb_find(map->impl.root, key);
    if (!node) {
        return -1;
    }
    rb_delete(&map->impl, node);
    return 0;
}

const char *cobalt_treemap_min_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = rb_find_min(map->impl.root);
    return node ? node->key : NULL;
}

const char *cobalt_treemap_max_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = rb_find_max(map->impl.root);
    return node ? node->key : NULL;
}

size_t cobalt_treemap_size(const cobalt_treemap_t *map)
{
    return map ? map->impl.size : 0;
}
