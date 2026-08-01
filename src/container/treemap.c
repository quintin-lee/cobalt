#include "cobalt/container/treemap.h"
#include <stdlib.h>
#include <string.h>

/* Red-black tree colors */
#define RB_RED   0
#define RB_BLACK 1

/* Tree node */
typedef struct rb_node {
    char *key;
    void *value;
    int color;
    struct rb_node *left;
    struct rb_node *right;
    struct rb_node *parent;
} rb_node_t;

/* Tree implementation */
typedef struct {
    rb_node_t *root;
    size_t size;
} treemap_impl_t;

struct cobalt_treemap {
    treemap_impl_t impl;
};

/* Helper: create new node */
static rb_node_t* rb_node_create(const char *key, void *value) {
    rb_node_t *node = malloc(sizeof(rb_node_t));
    if (!node) return NULL;
    node->key = strdup(key);
    if (!node->key) {
        free(node);
        return NULL;
    }
    node->value = value;
    node->color = RB_RED;
    node->left = node->right = node->parent = NULL;
    return node;
}

/* Helper: left rotate */
static void rb_rotate_left(rb_node_t **root, rb_node_t *node) {
    rb_node_t *right = node->right;
    if (!right) return;
    
    node->right = right->left;
    if (right->left) right->left->parent = node;
    right->parent = node->parent;
    
    if (!node->parent) *root = right;
    else if (node == node->parent->left) node->parent->left = right;
    else node->parent->right = right;
    
    right->left = node;
    node->parent = right;
}

/* Helper: right rotate */
static void rb_rotate_right(rb_node_t **root, rb_node_t *node) {
    rb_node_t *left = node->left;
    if (!left) return;
    
    node->left = left->right;
    if (left->right) left->right->parent = node;
    left->parent = node->parent;
    
    if (!node->parent) *root = left;
    else if (node == node->parent->right) node->parent->right = left;
    else node->parent->left = left;
    
    left->right = node;
    node->parent = left;
}

/* Helper: fix insert */
static void rb_insert_fixup(rb_node_t **root, rb_node_t *node) {
    rb_node_t *parent, *grandparent;
    
    while (node != *root && node->color == RB_RED) {
        parent = node->parent;
        grandparent = node->parent->parent;
        
        if (parent == grandparent->left) {
            rb_node_t *uncle = grandparent->right;
            
            if (uncle && uncle->color == RB_RED) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                grandparent->color = RB_RED;
                node = grandparent;
            } else {
                if (node == parent->right) {
                    node = parent;
                    rb_rotate_left(root, node);
                    parent = node->parent;
                    grandparent = node->parent->parent;
                }
                parent->color = RB_BLACK;
                grandparent->color = RB_RED;
                rb_rotate_right(root, grandparent);
            }
        } else {
            rb_node_t *uncle = grandparent->left;
            
            if (uncle && uncle->color == RB_RED) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                grandparent->color = RB_RED;
                node = grandparent;
            } else {
                if (node == parent->left) {
                    node = parent;
                    rb_rotate_right(root, node);
                    parent = node->parent;
                    grandparent = node->parent->parent;
                }
                parent->color = RB_BLACK;
                grandparent->color = RB_RED;
                rb_rotate_left(root, grandparent);
            }
        }
    }
    (*root)->color = RB_BLACK;
}

/* Helper: find minimum */
static rb_node_t* rb_min_node(rb_node_t *node) {
    while (node->left) node = node->left;
    return node;
}

/* Helper: find maximum */
static rb_node_t* rb_max_node(rb_node_t *node) {
    while (node->right) node = node->right;
    return node;
}

/* Helper: find successor */
static rb_node_t* rb_successor(rb_node_t *node) {
    if (node->right) return rb_min_node(node->right);
    rb_node_t *parent = node->parent;
    while (parent && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

/* Helper: transplant */
static void rb_transplant(rb_node_t **root, rb_node_t *u, rb_node_t *v) {
    if (!u->parent) *root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    if (v) v->parent = u->parent;
}

/* Helper: fix delete */
static void rb_delete_fixup(rb_node_t **root, rb_node_t *node) {
    rb_node_t *w;
    
    while (node != *root && node->color == RB_BLACK) {
        if (node == node->parent->left) {
            w = node->parent->right;
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                node->parent->color = RB_RED;
                rb_rotate_left(root, node->parent);
                w = node->parent->right;
            }
            if ((!w->left || w->left->color == RB_BLACK) &&
                (!w->right || w->right->color == RB_BLACK)) {
                w->color = RB_RED;
                node = node->parent;
            } else {
                if (!w->right || w->right->color == RB_BLACK) {
                    if (w->left) w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_rotate_right(root, w);
                    w = node->parent->right;
                }
                w->color = node->parent->color;
                node->parent->color = RB_BLACK;
                if (w->right) w->right->color = RB_BLACK;
                rb_rotate_left(root, node->parent);
                node = *root;
            }
        } else {
            w = node->parent->left;
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                node->parent->color = RB_RED;
                rb_rotate_right(root, node->parent);
                w = node->parent->left;
            }
            if ((!w->right || w->right->color == RB_BLACK) &&
                (!w->left || w->left->color == RB_BLACK)) {
                w->color = RB_RED;
                node = node->parent;
            } else {
                if (!w->left || w->left->color == RB_BLACK) {
                    if (w->right) w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_rotate_left(root, w);
                    w = node->parent->left;
                }
                w->color = node->parent->color;
                node->parent->color = RB_BLACK;
                if (w->left) w->left->color = RB_BLACK;
                rb_rotate_right(root, node->parent);
                node = *root;
            }
        }
    }
    node->color = RB_BLACK;
}

/* Public API */
cobalt_treemap_t *cobalt_treemap_create(void) {
    cobalt_treemap_t *map = malloc(sizeof(cobalt_treemap_t));
    if (!map) return NULL;
    map->impl.root = NULL;
    map->impl.size = 0;
    return map;
}

void cobalt_treemap_destroy(cobalt_treemap_t *map) {
    if (!map) return;
    
    rb_node_t *node = map->impl.root;
    rb_node_t *to_free;
    
    while (node) {
        if (node->left) {
            node = node->left;
        } else if (node->right) {
            node = node->right;
        } else {
            to_free = node;
            node = node->parent;
            free(to_free->key);
            free(to_free);
        }
    }
    
    free(map);
}

int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value) {
    if (!map || !key) return -1;
    
    rb_node_t **px = &map->impl.root;
    rb_node_t *parent = NULL;
    
    while (*px) {
        parent = *px;
        int cmp = strcmp(key, parent->key);
        if (cmp < 0) px = &parent->left;
        else if (cmp > 0) px = &parent->right;
        else {
            parent->value = value;
            return 0;
        }
    }
    
    rb_node_t *new_node = rb_node_create(key, value);
    if (!new_node) return -1;
    
    new_node->parent = parent;
    if (!parent) map->impl.root = new_node;
    else if (strcmp(key, parent->key) < 0) parent->left = new_node;
    else parent->right = new_node;
    
    rb_insert_fixup(&map->impl.root, new_node);
    map->impl.size++;
    
    return 0;
}

void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key) {
    if (!map || !key) return NULL;
    
    rb_node_t *node = map->impl.root;
    while (node) {
        int cmp = strcmp(key, node->key);
        if (cmp < 0) node = node->left;
        else if (cmp > 0) node = node->right;
        else return node->value;
    }
    return NULL;
}

int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key) {
    if (!map || !key) return -1;
    
    rb_node_t *node = map->impl.root;
    while (node) {
        int cmp = strcmp(key, node->key);
        if (cmp < 0) node = node->left;
        else if (cmp > 0) node = node->right;
        else break;
    }
    
    if (!node) return -1;
    
    rb_node_t *y = (node->left && node->right) ? rb_successor(node) : node;
    rb_node_t *x = y->left ? y->left : y->right;
    
    if (x) x->parent = y->parent;
    if (!y->parent) map->impl.root = x;
    else if (y == y->parent->left) y->parent->left = x;
    else y->parent->right = x;
    
    if (y != node) {
        node->key = y->key;
        node->value = y->value;
        y = node;
    }
    
    if (y->color == RB_BLACK) {
        rb_delete_fixup(&map->impl.root, x);
    }
    
    free(y->key);
    free(y);
    map->impl.size--;
    
    return 0;
}

const char *cobalt_treemap_min_key(cobalt_treemap_t *map) {
    if (!map || !map->impl.root) return NULL;
    return rb_min_node(map->impl.root)->key;
}

const char *cobalt_treemap_max_key(cobalt_treemap_t *map) {
    if (!map || !map->impl.root) return NULL;
    return rb_max_node(map->impl.root)->key;
}

size_t cobalt_treemap_size(cobalt_treemap_t *map) {
    if (!map) return 0;
    return map->impl.size;
}
