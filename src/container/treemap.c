/**
 * @file treemap.c
 * @brief Red-Black Tree implementation for TreeMap container
 */

#include "cobalt/container/treemap.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

#define RB_RED    0
#define RB_BLACK  1

typedef struct treemap_node {
    char                *key;
    void                *value;
    int                  color;
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

static treemap_node_t *rb_create_node(const char *key, void *value, int color)
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
    node->color = color;
    node->left = node->right = NULL;
    return node;
}

static void rb_destroy_tree(treemap_node_t *node)
{
    if (!node) return;
    rb_destroy_tree(node->left);
    rb_destroy_tree(node->right);
    free(node->key);
    free(node);
}

static treemap_node_t *rb_left_rotate(treemap_node_t *node)
{
    treemap_node_t *right = node->right;
    node->right = right->left;
    right->left = node;
    return right;
}

static treemap_node_t *rb_right_rotate(treemap_node_t *node)
{
    treemap_node_t *left = node->left;
    node->left = left->right;
    left->right = node;
    return left;
}

static void rb_flip_colors(treemap_node_t *node)
{
    node->color = !node->color;
    if (node->left) node->left->color = !node->left->color;
    if (node->right) node->right->color = !node->right->color;
}

static treemap_node_t *rb_insert_fixup(treemap_node_t *node)
{
    if (!node) return NULL;
    if (!node->left || !node->right) return node;

    if (node->right->color == RB_RED && node->left->color == RB_RED) {
        rb_flip_colors(node);
    }

    if (node->left->color == RB_RED && node->left->left->color == RB_RED) {
        node = rb_right_rotate(node);
        rb_flip_colors(node);
        if (node->right) rb_flip_colors(node->right);
    }

    if (node->right->color == RB_RED && node->left->color == RB_RED) {
        node = rb_left_rotate(node);
        rb_flip_colors(node);
        if (node->left) rb_flip_colors(node->left);
    }

    return node;
}

static treemap_node_t *rb_insert(treemap_node_t *node, const char *key, void *value)
{
    if (!node) {
        return rb_create_node(key, value, RB_RED);
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = rb_insert(node->left, key, value);
    } else if (cmp > 0) {
        node->right = rb_insert(node->right, key, value);
    } else {
        node->value = value;
        return node;
    }

    return rb_insert_fixup(node);
}

static treemap_node_t *rb_find_node(treemap_node_t *node, const char *key)
{
    if (!node) return NULL;
    int cmp = strcmp(key, node->key);
    if (cmp < 0) return rb_find_node(node->left, key);
    if (cmp > 0) return rb_find_node(node->right, key);
    return node;
}

static treemap_node_t *rb_find_min(treemap_node_t *node)
{
    while (node && node->left) node = node->left;
    return node;
}

static treemap_node_t *rb_find_max(treemap_node_t *node)
{
    while (node && node->right) node = node->right;
    return node;
}

static int rb_is_red(treemap_node_t *node)
{
    return node && node->color == RB_RED;
}

static treemap_node_t *rb_move_red_left(treemap_node_t *node)
{
    rb_flip_colors(node);
    if (node->right && node->right->left && node->right->left->color == RB_RED) {
        node->right = rb_right_rotate(node->right);
        node = rb_left_rotate(node);
        rb_flip_colors(node);
    }
    return node;
}

static treemap_node_t *rb_move_red_right(treemap_node_t *node)
{
    rb_flip_colors(node);
    if (node->left && node->left->left && node->left->left->color == RB_RED) {
        node = rb_right_rotate(node);
        rb_flip_colors(node);
    }
    return node;
}

static treemap_node_t *rb_delete_fixup(treemap_node_t *node);

static treemap_node_t *rb_delete_min(treemap_node_t *node)
{
    if (!node || !node->left) return NULL;

    if (!rb_is_red(node->left) && !(rb_is_red(node->left->left))) {
        node = rb_move_red_left(node);
    }

    node->left = rb_delete_min(node->left);
    return rb_delete_fixup(node);
}

static treemap_node_t *rb_delete_fixup(treemap_node_t *node)
{
    if (!node) return NULL;

    if (rb_is_red(node->left)) {
        node = rb_right_rotate(node);
    }
    if (rb_is_red(node->right)) {
        node = rb_left_rotate(node);
    }
    return node;
}

static treemap_node_t *rb_delete(treemap_node_t *node, const char *key)
{
    if (!node) return NULL;

    if (strcmp(key, node->key) < 0) {
        if (!rb_is_red(node->left) && !(rb_is_red(node->left->left))) {
            node = rb_move_red_left(node);
        }
        node->left = rb_delete(node->left, key);
    } else {
        if (rb_is_red(node->left)) {
            node = rb_right_rotate(node);
        }
        if (!rb_is_red(node->right) && !(rb_is_red(node->right->left))) {
            node = rb_move_red_right(node);
        }
        if (strcmp(key, node->key) == 0) {
            treemap_node_t *successor = rb_find_min(node->right);
            if (successor) {
                char *old_key = node->key;
                node->key = cobalt_strdup(successor->key);
                node->value = successor->value;
                free(old_key);
                node->right = rb_delete_min(node->right);
            }
        } else {
            node->right = rb_delete(node->right, key);
        }
    }
    return rb_delete_fixup(node);
}

cobalt_treemap_t *cobalt_treemap_create(void)
{
    cobalt_treemap_t *map = malloc(sizeof(cobalt_treemap_t));
    if (!map) return NULL;
    map->impl.root = NULL;
    map->impl.size = 0;
    return map;
}

void cobalt_treemap_destroy(cobalt_treemap_t *map)
{
    if (!map) return;
    if (map->impl.root) {
        rb_destroy_tree(map->impl.root);
    }
    free(map);
}

int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value)
{
    if (!map || !key) return -1;

    if (rb_find_node(map->impl.root, key)) {
        map->impl.root = rb_insert(map->impl.root, key, value);
    } else {
        map->impl.root = rb_insert(map->impl.root, key, value);
        map->impl.size++;
    }
    if (map->impl.root) {
        map->impl.root->color = RB_BLACK;
    }
    return 0;
}

void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) return NULL;
    treemap_node_t *node = rb_find_node(map->impl.root, key);
    return node ? node->value : NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) return -1;
    if (!rb_find_node(map->impl.root, key)) return -1;

    int was_black = map->impl.root->color == RB_BLACK;
    map->impl.root = rb_delete(map->impl.root, key);
    if (map->impl.root) {
        map->impl.root->color = RB_BLACK;
    }
    if (was_black) {
        map->impl.size--;
    }
    return 0;
}

const char *cobalt_treemap_min_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) return NULL;
    treemap_node_t *node = rb_find_min(map->impl.root);
    return node ? node->key : NULL;
}

const char *cobalt_treemap_max_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) return NULL;
    treemap_node_t *node = rb_find_max(map->impl.root);
    return node ? node->key : NULL;
}

size_t cobalt_treemap_size(cobalt_treemap_t *map)
{
    return map ? map->impl.size : 0;
}
#include "cobalt/platform/debug_assert.h"
