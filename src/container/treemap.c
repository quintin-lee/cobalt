/**
 * @file treemap.c
 * @brief Red-Black Tree implementation for TreeMap container
 */

#include "cobalt/container/treemap.h"
#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/debug_assert.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

#define RB_RED 0
#define RB_BLACK 1

/**
 * @brief Red-black tree node holding one key-value entry
 */
typedef struct treemap_node {
    void                *key;       /**< Key pointer (owned copy when key_owned) */
    void                *value;     /**< Value pointer stored */
    struct treemap_node *left;      /**< Left child */
    struct treemap_node *right;     /**< Right child */
    struct treemap_node *parent;    /**< Parent node (NULL for root) */
    int                  color;     /**< RB_RED or RB_BLACK */
    int                  key_owned; /**< Non-zero when key memory is owned */
} treemap_node_t;

/**
 * @brief Internal treemap state shared by all operations
 */
typedef struct {
    treemap_node_t       *root; /**< Root of the red-black tree */
    size_t                size; /**< Number of entries stored */
    cobalt_allocator_t   *alloc;
    cobalt_compare_func_t compare_func; /**< Key ordering function */
} treemap_impl_t;

/**
 * @brief Opaque treemap structure
 * @details Embeds cobalt_map_t as first member for map polymorphism.
 */
struct cobalt_treemap {
    cobalt_map_t   base; /**< Map interface (polymorphic base) */
    treemap_impl_t impl;
};

/* ========================================================================= */
/* Forward declarations                                                       */
/* ========================================================================= */
static int             rb_compare(const treemap_impl_t *impl, const void *a, const void *b);
static treemap_node_t *rb_find(treemap_node_t *node, const void *key, const treemap_impl_t *impl);
static treemap_node_t *
rb_node_create_ext(treemap_impl_t *impl, const void *key, void *value, int owned);

/* ========================================================================= */
/* Node helpers                                                               */
/* ========================================================================= */

/**
 * @brief allocate node with owned copy of string key
 * @param impl tree state holding allocator
 * @param key string key to duplicate
 * @param value value pointer to store
 * @return new node, or NULL on allocation failure
 */
static treemap_node_t *rb_node_create(treemap_impl_t *impl, const char *key, void *value)
{
    return rb_node_create_ext(impl, key, value, 1);
}

/**
 * @brief allocate node with optional key ownership
 * @param impl tree state holding allocator
 * @param key key pointer to store or duplicate
 * @param value value pointer to store
 * @param owned nonzero to duplicate key, zero to borrow pointer
 * @return new node, or NULL on allocation failure
 */
static treemap_node_t *
rb_node_create_ext(treemap_impl_t *impl, const void *key, void *value, int owned)
{
    if (!impl) {
        return NULL;
    }
    treemap_node_t *node =
        (treemap_node_t *)impl->alloc->alloc(impl->alloc, sizeof(treemap_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    if (owned) {
        node->key = cobalt_strdup_with_alloc((const char *)key, impl->alloc);
        if (!node->key) {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            impl->alloc->free(impl->alloc, node);
            return NULL;
        }
    } else {
        node->key = (void *)key;
    }
    node->value     = value;
    node->key_owned = owned;
    node->left = node->right = node->parent = NULL;
    node->color                             = RB_RED;
    return node;
}

/**
 * @brief release node and owned key back to allocator
 * @param impl tree state holding allocator
 * @param node node to free
 */
static void rb_node_free(treemap_impl_t *impl, treemap_node_t *node)
{
    if (!node || !impl) {
        return;
    }
    if (node->key && node->key_owned) {
        impl->alloc->free(impl->alloc, node->key);
    }
    impl->alloc->free(impl->alloc, node);
}

/**
 * @brief recursively free subtree in post order
 * @param impl tree state holding allocator
 * @param node subtree root to destroy
 */
static void rb_destroy_tree(treemap_impl_t *impl, treemap_node_t *node)
{
    if (!node || !impl) {
        return;
    }
    rb_destroy_tree(impl, node->left);
    rb_destroy_tree(impl, node->right);
    rb_node_free(impl, node);
}

/* ========================================================================= */
/* Rotations — both take the full impl so they can update the root pointer   */
/* ========================================================================= */

/**
 * @brief rotate subtree left around node
 * @param tree tree state whose root pointer may change
 * @param x rotation pivot
 */
static void rb_rotate_left(treemap_impl_t *tree, treemap_node_t *x)
{
    treemap_node_t *y = x->right;
    x->right          = y->left;
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
    y->left   = x;
    x->parent = y;
}

/**
 * @brief rotate subtree right around node
 * @param tree tree state whose root pointer may change
 * @param x rotation pivot
 */
static void rb_rotate_right(treemap_impl_t *tree, treemap_node_t *x)
{
    treemap_node_t *y = x->left;
    x->left           = y->right;
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
    y->right  = x;
    x->parent = y;
}

/* ========================================================================= */
/* Insert fixup                                                               */
/* ========================================================================= */

/**
 * @brief restore red-black invariants after insertion
 * @param tree tree state whose root pointer may change
 * @param z newly inserted node
 */
static void rb_insert_fixup(treemap_impl_t *tree, treemap_node_t *z)
{
    while (z->parent && z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            treemap_node_t *uncle = z->parent->parent->right;
            if (uncle && uncle->color == RB_RED) {
                z->parent->color         = RB_BLACK;
                uncle->color             = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z                        = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rb_rotate_left(tree, z);
                }
                z->parent->color         = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_right(tree, z->parent->parent);
            }
        } else {
            treemap_node_t *uncle = z->parent->parent->left;
            if (uncle && uncle->color == RB_RED) {
                z->parent->color         = RB_BLACK;
                uncle->color             = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z                        = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rb_rotate_right(tree, z);
                }
                z->parent->color         = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_left(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = RB_BLACK;
}

/**
 * @brief insert node into binary search position and rebalance
 * @param tree tree state to modify
 * @param z node to insert
 * @note duplicate key updates stored value and discards new node
 */
static void rb_insert(treemap_impl_t *tree, treemap_node_t *z)
{
    treemap_node_t *y = NULL;
    treemap_node_t *x = tree->root;

    while (x) {
        y       = x;
        int cmp = rb_compare(tree, z->key, x->key);
        if (cmp < 0) {
            x = x->left;
        } else if (cmp > 0) {
            x = x->right;
        } else {
            /* Duplicate key — update value, discard new node */
            x->value = z->value;
            rb_node_free(tree, z);
            return;
        }
    }
    z->parent = y;
    if (!y) {
        tree->root = z;
    } else {
        int cmp = rb_compare(tree, z->key, y->key);
        if (cmp < 0) {
            y->left = z;
        } else {
            y->right = z;
        }
    }
    rb_insert_fixup(tree, z);
    tree->size++;
}

/* ========================================================================= */
/* Delete fixup                                                               */
/* ========================================================================= */

/**
 * @brief transplant subtree replacing old node with new node
 * @param tree tree state whose root pointer may change
 * @param old_node node being removed
 * @param new_node replacement subtree, may be NULL
 */
static void rb_replace(treemap_impl_t *tree, treemap_node_t *old_node, treemap_node_t *new_node)
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

/**
 * @brief restore red-black invariants after deletion
 * @param tree tree state whose root pointer may change
 * @param x child that replaced removed black node, may be NULL
 */
static void rb_delete_fixup(treemap_impl_t *tree, treemap_node_t *x)
{
    while (x && x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            treemap_node_t *sibling = x->parent->right;
            if (sibling->color == RB_RED) {
                sibling->color   = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_left(tree, x->parent);
                sibling = x->parent->right;
            }
            if ((!sibling->left || sibling->left->color == RB_BLACK) &&
                (!sibling->right || sibling->right->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x              = x->parent;
            } else {
                if (!sibling->right || sibling->right->color == RB_BLACK) {
                    if (sibling->left) {
                        sibling->left->color = RB_BLACK;
                    }
                    sibling->color = RB_RED;
                    rb_rotate_right(tree, sibling);
                    sibling = x->parent->right;
                }
                sibling->color   = x->parent->color;
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
                sibling->color   = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_right(tree, x->parent);
                sibling = x->parent->left;
            }
            if ((!sibling->right || sibling->right->color == RB_BLACK) &&
                (!sibling->left || sibling->left->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x              = x->parent;
            } else {
                if (!sibling->left || sibling->left->color == RB_BLACK) {
                    if (sibling->right) {
                        sibling->right->color = RB_BLACK;
                    }
                    sibling->color = RB_RED;
                    rb_rotate_left(tree, sibling);
                    sibling = x->parent->left;
                }
                sibling->color   = x->parent->color;
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

/**
 * @brief unlink node from tree, rebalance and free it
 * @param tree tree state to modify
 * @param z node to delete, must belong to tree
 */
static void rb_delete(treemap_impl_t *tree, treemap_node_t *z)
{
    treemap_node_t *y                = z;
    int             y_original_color = y->color;
    treemap_node_t *x                = NULL;

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
        x                = y->right;
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

    rb_node_free(tree, z);
    tree->size--;

    if (y_original_color == RB_BLACK) {
        rb_delete_fixup(tree, x);
    }
}

/* ========================================================================= */
/* Find helpers                                                               */
/* ========================================================================= */

/**
 * @brief compare two keys with custom comparator or strcmp fallback
 * @param impl tree state holding optional comparator
 * @param a first key
 * @param b second key
 * @return negative if a < b, zero if equal, positive if a > b
 */
static int rb_compare(const treemap_impl_t *impl, const void *a, const void *b)
{
    if (impl->compare_func) {
        return impl->compare_func(a, b);
    }
    return strcmp((const char *)a, (const char *)b);
}

/**
 * @brief search subtree for node holding key
 * @param node subtree root to search
 * @param key key to find
 * @param impl tree state holding comparator
 * @return matching node, or NULL when absent
 */
static treemap_node_t *rb_find(treemap_node_t *node, const void *key, const treemap_impl_t *impl)
{
    while (node) {
        int cmp = rb_compare(impl, key, node->key);
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

/**
 * @brief descend to leftmost node of subtree
 * @param node subtree root to search
 * @return minimum node, or NULL for empty subtree
 */
static treemap_node_t *rb_find_min(treemap_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

/**
 * @brief descend to rightmost node of subtree
 * @param node subtree root to search
 * @return maximum node, or NULL for empty subtree
 */
static treemap_node_t *rb_find_max(treemap_node_t *node)
{
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

/* ========================================================================= */
/* In-order iterator (sorted by key)                                         */
/* ========================================================================= */

/**
 * @brief Iterator state for in-order key-sorted traversal
 */
typedef struct {
    treemap_impl_t     *impl;      /**< Backing tree state */
    treemap_node_t    **stack;     /**< Ancestor stack for traversal */
    size_t              stack_top; /**< Depth of the traversal stack */
    size_t              stack_cap; /**< Allocated stack capacity */
    int                 finished;  /**< Non-zero once traversal is exhausted */
    cobalt_allocator_t *alloc;
} treemap_map_iter_t;

/**
 * @brief push node onto iterator traversal stack growing it as needed
 * @param iter iterator holding stack
 * @param node node to push
 * @note marks iterator finished when stack growth fails
 */
static void stack_push(treemap_map_iter_t *iter, treemap_node_t *node)
{
    if (iter->stack_top == iter->stack_cap) {
        size_t           new_cap = iter->stack_cap == 0 ? 16 : iter->stack_cap * 2;
        treemap_node_t **ns      = (treemap_node_t **)iter->alloc->realloc(
            iter->alloc, iter->stack, new_cap * sizeof(treemap_node_t *));
        if (!ns) {
            iter->finished = 1;
            return;
        }
        iter->stack     = ns;
        iter->stack_cap = new_cap;
    }
    iter->stack[iter->stack_top++] = node;
}

/**
 * @brief report whether iterator has remaining pairs
 * @param ctx iterator state
 * @return nonzero while pairs remain
 */
static int treemap_map_iter_has_next(void *ctx)
{
    treemap_map_iter_t *iter = (treemap_map_iter_t *)ctx;
    return !iter->finished;
}

/**
 * @brief yield next key-value pair in sorted key order
 * @param ctx iterator state
 * @return next pair, or empty pair when exhausted
 */
static cobalt_map_pair_t treemap_map_iter_next(void *ctx)
{
    treemap_map_iter_t *iter = (treemap_map_iter_t *)ctx;
    cobalt_map_pair_t   pair = {NULL, NULL};

    if (iter->finished) {
        return pair;
    }

    if (iter->stack_top == 0) {
        treemap_node_t *cur = iter->impl->root;
        while (cur) {
            stack_push(iter, cur);
            cur = cur->left;
        }
    }

    if (iter->stack_top == 0) {
        iter->finished = 1;
        return pair;
    }

    treemap_node_t *node = iter->stack[--iter->stack_top];
    pair.key             = node->key;
    pair.value           = node->value;

    if (node->right) {
        treemap_node_t *cur = node->right;
        while (cur) {
            stack_push(iter, cur);
            cur = cur->left;
        }
    } else if (iter->stack_top == 0) {
        iter->finished = 1;
    }

    return pair;
}

/**
 * @brief release iterator stack and state
 * @param ctx iterator state to destroy
 */
static void treemap_map_iter_destroy(void *ctx)
{
    if (ctx) {
        treemap_map_iter_t *iter = (treemap_map_iter_t *)ctx;
        iter->alloc->free(iter->alloc, iter->stack);
        iter->alloc->free(iter->alloc, iter);
    }
}

static const cobalt_map_iterator_vtable_t treemap_map_iter_vtable = {
    .has_next = treemap_map_iter_has_next,
    .next     = treemap_map_iter_next,
    .destroy  = treemap_map_iter_destroy,
};

/**
 * @brief create in-order iterator over map entries
 * @param self map interface pointer
 * @return new iterator, or NULL on allocation failure
 */
static cobalt_map_iterator_t *treemap_map_iterator(cobalt_map_t *self)
{
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;

    treemap_map_iter_t *iter_state =
        (treemap_map_iter_t *)map->impl.alloc->alloc(map->impl.alloc, sizeof(treemap_map_iter_t));
    if (iter_state) {
        memset(iter_state, 0, sizeof(treemap_map_iter_t));
        iter_state->impl      = &map->impl;
        iter_state->stack     = NULL;
        iter_state->stack_top = 0;
        iter_state->stack_cap = 0;
        iter_state->finished  = 0;
        iter_state->alloc     = map->impl.alloc;
    }
    if (!iter_state) {
        return NULL;
    }
    cobalt_map_iterator_t *iter = (cobalt_map_iterator_t *)map->impl.alloc->alloc(
        map->impl.alloc, sizeof(cobalt_map_iterator_t));
    if (!iter) {
        map->impl.alloc->free(map->impl.alloc, iter_state);
        return NULL;
    }
    iter->vtable = &treemap_map_iter_vtable;
    iter->data   = iter_state;
    iter->alloc  = map->impl.alloc;
    return iter;
}

/* ========================================================================= */
/* Map interface functions                                                  */
/* ========================================================================= */

/**
 * @brief look up value through map interface slot
 * @param self map interface pointer
 * @param key key pointer
 * @param key_len unused key length
 * @return value pointer, or NULL when absent
 */
static void *treemap_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    (void)key_len;
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return cobalt_treemap_get(map, (const char *)key);
}

/**
 * @brief insert value through map interface slot
 * @param self map interface pointer
 * @param key key pointer
 * @param key_len unused key length
 * @param value value pointer to store
 * @return 0 on success, -1 on failure
 */
static int treemap_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    (void)key_len;
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return cobalt_treemap_put(map, (const char *)key, value);
}

/**
 * @brief remove entry through map interface slot
 * @param self map interface pointer
 * @param key key pointer
 * @param key_len unused key length
 * @return 0 on success, -1 when key absent
 */
static int treemap_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    (void)key_len;
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return cobalt_treemap_remove(map, (const char *)key);
}

/**
 * @brief report entry count through map interface slot
 * @param self map interface pointer
 * @return number of entries
 */
static size_t treemap_map_size(cobalt_map_t *self)
{
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return cobalt_treemap_size(map);
}

/**
 * @brief report emptiness through map interface slot
 * @param self map interface pointer
 * @return nonzero when map holds no entries
 */
static int treemap_map_is_empty(cobalt_map_t *self)
{
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return cobalt_treemap_size(map) == 0;
}

/**
 * @brief destroy map through map interface slot
 * @param self map interface pointer
 */
static void treemap_map_destroy(cobalt_map_t *self)
{
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    cobalt_treemap_destroy(map);
}

/**
 * @brief test key presence through map interface slot
 * @param self map interface pointer
 * @param key key pointer
 * @param key_len unused key length
 * @return nonzero when key exists
 */
static int treemap_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    (void)key_len;
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    return rb_find(map->impl.root, key, &map->impl) != NULL;
}

/**
 * @brief remove all entries through map interface slot
 * @param self map interface pointer
 */
static void treemap_map_clear(cobalt_map_t *self)
{
    cobalt_treemap_t *map = (cobalt_treemap_t *)self;
    if (map->impl.root) {
        rb_destroy_tree(&map->impl, map->impl.root);
        map->impl.root = NULL;
    }
    map->impl.size = 0;
}

static const cobalt_map_t treemap_map_vtable = {
    .get      = treemap_map_get,
    .put      = treemap_map_put,
    .remove   = treemap_map_remove,
    .contains = treemap_map_contains,
    .clear    = treemap_map_clear,
    .size     = treemap_map_size,
    .is_empty = treemap_map_is_empty,
    .iterator = treemap_map_iterator,
    .destroy  = treemap_map_destroy,
};

/* ========================================================================= */
/* Public iterator factory                                                    */
/* ========================================================================= */

/// @brief Create a map iterator for this tree map
/// @param map Tree map instance
/// @return Iterator pointer, or NULL on failure
/// @note Returns a cobalt_map_iterator_t compatible with the Map interface.
///       Destroy with cobalt_map_iterator_destroy().
cobalt_map_iterator_t *cobalt_treemap_iterator_create(cobalt_treemap_t *map)
{
    if (!map) {
        return NULL;
    }
    return treemap_map_iterator((cobalt_map_t *)map);
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

cobalt_treemap_t *cobalt_treemap_create_with_allocator(cobalt_allocator_t *alloc);
/**
 * @brief create empty tree map using system allocator
 * @return new map, or NULL on allocation failure
 */
cobalt_treemap_t *cobalt_treemap_create(void)
{
    return cobalt_treemap_create_with_allocator(cobalt_allocator_get_system());
}

/**
 * @brief create empty tree map with custom key comparator
 * @param compare_func comparator used instead of strcmp, may be NULL
 * @return new map, or NULL on allocation failure
 */
cobalt_treemap_t *cobalt_treemap_create_ext(cobalt_compare_func_t compare_func)
{
    cobalt_treemap_t *map = cobalt_treemap_create();
    if (!map) {
        return NULL;
    }
    map->impl.compare_func = compare_func;
    return map;
}

/**
 * @brief create empty tree map using given allocator
 * @param alloc allocator backing map and nodes
 * @return new map, or NULL on allocation failure
 */
cobalt_treemap_t *cobalt_treemap_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_treemap_t *map = (cobalt_treemap_t *)alloc->alloc(alloc, sizeof(cobalt_treemap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    map->base              = treemap_map_vtable;
    map->impl.root         = NULL;
    map->impl.size         = 0;
    map->impl.alloc        = alloc;
    map->impl.compare_func = NULL;
    return map;
}

/**
 * @brief destroy map and release all nodes
 * @param map map to destroy, NULL is no-op
 */
void cobalt_treemap_destroy(cobalt_treemap_t *map)
{
    if (!map) {
        return;
    }
    if (map->impl.root) {
        rb_destroy_tree(&map->impl, map->impl.root);
    }
    map->impl.alloc->free(map->impl.alloc, map);
}

/**
 * @brief insert or replace value stored under key
 * @param map tree map instance
 * @param key string key to copy
 * @param value value pointer to store
 * @return 0 on success, -1 on failure
 */
int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *existing = rb_find(map->impl.root, key, &map->impl);
    if (existing) {
        existing->value = value;
        return 0;
    }
    treemap_node_t *node = rb_node_create(&map->impl, key, value);
    if (!node) {
        return -1;
    }
    rb_insert(&map->impl, node);
    return 0;
}

/**
 * @brief look up value stored under key
 * @param map tree map instance
 * @param key string key to search
 * @return value pointer, or NULL when absent
 */
void *cobalt_treemap_get(const cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return NULL;
    }
    treemap_node_t *node = rb_find(map->impl.root, key, &map->impl);
    return node ? node->value : NULL;
}

/**
 * @brief remove entry stored under key
 * @param map tree map instance
 * @param key string key to remove
 * @return 0 on success, -1 when key absent
 */
int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *node = rb_find(map->impl.root, key, &map->impl);
    if (!node) {
        return -1;
    }
    rb_delete(&map->impl, node);
    return 0;
}

/**
 * @brief fetch smallest key in sorted order
 * @param map tree map instance
 * @return smallest key, or NULL when map is empty
 */
const char *cobalt_treemap_min_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = rb_find_min(map->impl.root);
    return node ? node->key : NULL;
}

/**
 * @brief fetch largest key in sorted order
 * @param map tree map instance
 * @return largest key, or NULL when map is empty
 */
const char *cobalt_treemap_max_key(cobalt_treemap_t *map)
{
    if (!map || !map->impl.root) {
        return NULL;
    }
    treemap_node_t *node = rb_find_max(map->impl.root);
    return node ? node->key : NULL;
}

/**
 * @brief report number of entries in map
 * @param map tree map instance
 * @return entry count, zero for NULL map
 */
size_t cobalt_treemap_size(const cobalt_treemap_t *map)
{
    return map ? map->impl.size : 0;
}
