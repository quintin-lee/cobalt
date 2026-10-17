#include "cobalt/container/lfskiplist.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Internal SkipList node layout with flexible array of marked pointers
 */
typedef struct cobalt_lfskiplist_node {
    void              *key;
    _Atomic(void *)    value;
    uint8_t            top_level;
    _Atomic(uintptr_t) next[];
} cobalt_lfskiplist_node_t;

/**
 * @brief Lock-free skip list container
 */
struct cobalt_lfskiplist {
    cobalt_lfskiplist_node_t *head;
    compare_func_t            compar;
    cobalt_allocator_t       *allocator;
    cobalt_ebr_t             *ebr;
    bool                      owns_ebr;
    atomic_size_t             size;
};

/**
 * @brief Range iterator holding pinned EBR epoch
 */
struct cobalt_lfskiplist_iter {
    cobalt_lfskiplist_t      *skiplist;
    cobalt_ebr_handle_t      *handle;
    cobalt_lfskiplist_node_t *curr;
    const void               *max_key;
    bool                      finished;
};

typedef struct {
    uint64_t             ebr_id;
    cobalt_ebr_handle_t *handle;
} tls_ebr_cache_t;

static _Thread_local tls_ebr_cache_t tl_ebr_cache = {0, NULL};

static void *sl_alloc(cobalt_lfskiplist_t *sl, size_t size)
{
    if (sl && sl->allocator && sl->allocator->alloc) {
        return sl->allocator->alloc(sl->allocator, size);
    }
    return malloc(size);
}

static void sl_free(cobalt_lfskiplist_t *sl, void *ptr)
{
    if (!ptr) {
        return;
    }
    if (sl && sl->allocator && sl->allocator->free) {
        sl->allocator->free(sl->allocator, ptr);
    } else {
        free(ptr);
    }
}

static cobalt_ebr_handle_t *get_tls_ebr_handle(cobalt_lfskiplist_t *sl)
{
    uint64_t id = cobalt_ebr_get_id(sl->ebr);
    if (tl_ebr_cache.ebr_id == id && tl_ebr_cache.handle != NULL) {
        return tl_ebr_cache.handle;
    }
    cobalt_ebr_handle_t *h = cobalt_ebr_register(sl->ebr);
    tl_ebr_cache.ebr_id    = id;
    tl_ebr_cache.handle    = h;
    return h;
}

static cobalt_lfskiplist_node_t *
node_create(cobalt_lfskiplist_t *sl, void *key, void *value, uint8_t height)
{
    size_t size = sizeof(cobalt_lfskiplist_node_t) + height * sizeof(_Atomic(uintptr_t));
    cobalt_lfskiplist_node_t *n = (cobalt_lfskiplist_node_t *)sl_alloc(sl, size);
    if (!n) {
        return NULL;
    }
    n->key = key;
    atomic_init(&n->value, value);
    n->top_level = height;
    for (uint8_t i = 0; i < height; i++) {
        atomic_init(&n->next[i], 0);
    }
    return n;
}

static void node_ebr_free_fn(void *ptr, void *user_data)
{
    cobalt_lfskiplist_t *sl = (cobalt_lfskiplist_t *)user_data;
    sl_free(sl, ptr);
}

static uint8_t random_level(void)
{
    static _Thread_local uint32_t seed = 0;
    if (seed == 0) {
        seed = (uint32_t)(uintptr_t)&seed ^ 0x9e3779b9U;
        if (seed == 0) {
            seed = 1;
        }
    }
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    uint8_t  lvl  = 1;
    uint32_t bits = seed;
    while ((bits & 1) && lvl < COBALT_LFSKIPLIST_MAX_LEVEL) {
        lvl++;
        bits >>= 1;
    }
    return lvl;
}

static bool find_preds_succs(cobalt_lfskiplist_t      *sl,
                             const void               *key,
                             cobalt_lfskiplist_node_t *preds[COBALT_LFSKIPLIST_MAX_LEVEL],
                             cobalt_lfskiplist_node_t *succs[COBALT_LFSKIPLIST_MAX_LEVEL])
{
retry:;
    cobalt_lfskiplist_node_t *pred = sl->head;
    for (int level = COBALT_LFSKIPLIST_MAX_LEVEL - 1; level >= 0; level--) {
        uintptr_t curr_tagged = atomic_load_explicit(&pred->next[level], memory_order_acquire);
        cobalt_lfskiplist_node_t *curr =
            (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(curr_tagged);

        while (curr != NULL) {
            uintptr_t succ_tagged = atomic_load_explicit(&curr->next[level], memory_order_acquire);

            /* Cooperatively unlink marked nodes */
            while (COBALT_IS_MARKED(succ_tagged)) {
                uintptr_t expected = (uintptr_t)curr;
                uintptr_t desired  = (uintptr_t)COBALT_GET_UNMARKED(succ_tagged);

                if (!atomic_compare_exchange_weak_explicit(&pred->next[level],
                                                           &expected,
                                                           desired,
                                                           memory_order_acq_rel,
                                                           memory_order_acquire)) {
                    goto retry;
                }

                curr = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(succ_tagged);
                if (curr == NULL) {
                    break;
                }
                succ_tagged = atomic_load_explicit(&curr->next[level], memory_order_acquire);
            }

            if (curr == NULL) {
                break;
            }

            int cmp = sl->compar(curr->key, key);
            if (cmp < 0) {
                pred        = curr;
                curr_tagged = succ_tagged;
                curr        = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(curr_tagged);
            } else {
                break;
            }
        }

        preds[level] = pred;
        succs[level] = curr;
    }

    return (succs[0] != NULL && sl->compar(succs[0]->key, key) == 0);
}

cobalt_lfskiplist_t *cobalt_lfskiplist_create(compare_func_t compar)
{
    return cobalt_lfskiplist_create_with_allocator(compar, NULL, NULL);
}

cobalt_lfskiplist_t *cobalt_lfskiplist_create_with_allocator(compare_func_t      compar,
                                                             cobalt_allocator_t *alloc,
                                                             cobalt_ebr_t       *ebr)
{
    if (!compar) {
        return NULL;
    }

    cobalt_allocator_t  *effective_alloc = alloc ? alloc : cobalt_allocator_get_system();
    cobalt_lfskiplist_t *sl              = NULL;

    if (effective_alloc && effective_alloc->alloc) {
        sl = (cobalt_lfskiplist_t *)effective_alloc->alloc(effective_alloc,
                                                           sizeof(cobalt_lfskiplist_t));
    } else {
        sl = (cobalt_lfskiplist_t *)malloc(sizeof(cobalt_lfskiplist_t));
    }

    if (!sl) {
        return NULL;
    }

    sl->compar    = compar;
    sl->allocator = effective_alloc;
    atomic_init(&sl->size, 0);

    if (ebr) {
        sl->ebr      = ebr;
        sl->owns_ebr = false;
    } else {
        sl->ebr = cobalt_ebr_create(effective_alloc);
        if (!sl->ebr) {
            sl_free(sl, sl);
            return NULL;
        }
        sl->owns_ebr = true;
    }

    /* Head sentinel node with maximum height */
    sl->head = node_create(sl, NULL, NULL, COBALT_LFSKIPLIST_MAX_LEVEL);
    if (!sl->head) {
        if (sl->owns_ebr) {
            cobalt_ebr_destroy(sl->ebr);
        }
        sl_free(sl, sl);
        return NULL;
    }

    return sl;
}

void cobalt_lfskiplist_destroy(cobalt_lfskiplist_t *sl)
{
    if (!sl) {
        return;
    }

    /* Free all nodes starting from level 0 */
    cobalt_lfskiplist_node_t *curr =
        (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(atomic_load(&sl->head->next[0]));
    while (curr) {
        cobalt_lfskiplist_node_t *next =
            (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(atomic_load(&curr->next[0]));
        sl_free(sl, curr);
        curr = next;
    }

    sl_free(sl, sl->head);

    if (sl->owns_ebr && sl->ebr) {
        uint64_t id = cobalt_ebr_get_id(sl->ebr);
        if (tl_ebr_cache.ebr_id == id) {
            tl_ebr_cache.ebr_id = 0;
            tl_ebr_cache.handle = NULL;
        }
        cobalt_ebr_destroy(sl->ebr);
        sl->ebr = NULL;
    }

    sl_free(sl, sl);
}

int cobalt_lfskiplist_insert(cobalt_lfskiplist_t *sl, void *key, void *value)
{
    if (!sl || !key) {
        return -2;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(sl);
    cobalt_ebr_enter(h);

    cobalt_lfskiplist_node_t *preds[COBALT_LFSKIPLIST_MAX_LEVEL];
    cobalt_lfskiplist_node_t *succs[COBALT_LFSKIPLIST_MAX_LEVEL];
    cobalt_lfskiplist_node_t *new_node  = NULL;
    uint8_t                   top_level = 0;

    while (1) {
        if (find_preds_succs(sl, key, preds, succs)) {
            if (new_node) {
                sl_free(sl, new_node);
            }
            cobalt_ebr_exit(h);
            return -1; /* Key already exists */
        }

        if (!new_node) {
            top_level = random_level();
            new_node  = node_create(sl, key, value, top_level);
            if (!new_node) {
                cobalt_ebr_exit(h);
                return -2; /* OOM */
            }
        }

        for (uint8_t i = 0; i < top_level; i++) {
            atomic_store_explicit(&new_node->next[i], (uintptr_t)succs[i], memory_order_relaxed);
        }

        /* Linearization point: Level 0 CAS */
        uintptr_t expected = (uintptr_t)succs[0];
        uintptr_t desired  = (uintptr_t)new_node;

        if (!atomic_compare_exchange_strong_explicit(&preds[0]->next[0],
                                                     &expected,
                                                     desired,
                                                     memory_order_release,
                                                     memory_order_relaxed)) {
            continue;
        }

        break;
    }

    /* Link levels 1 .. top_level - 1 */
    for (uint8_t i = 1; i < top_level; i++) {
        while (1) {
            uintptr_t my_next = atomic_load_explicit(&new_node->next[i], memory_order_acquire);
            if (COBALT_IS_MARKED(my_next)) {
                goto done;
            }

            uintptr_t expected = (uintptr_t)succs[i];
            uintptr_t desired  = (uintptr_t)new_node;

            if (atomic_compare_exchange_strong_explicit(&preds[i]->next[i],
                                                        &expected,
                                                        desired,
                                                        memory_order_release,
                                                        memory_order_relaxed)) {
                break;
            }

            find_preds_succs(sl, key, preds, succs);
            atomic_store_explicit(&new_node->next[i], (uintptr_t)succs[i], memory_order_relaxed);
        }
    }

done:
    atomic_fetch_add_explicit(&sl->size, 1, memory_order_relaxed);
    cobalt_ebr_exit(h);
    return 0;
}

int cobalt_lfskiplist_put(cobalt_lfskiplist_t *sl, void *key, void *value, void **out_old_value)
{
    if (!sl || !key) {
        return -1;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(sl);
    cobalt_ebr_enter(h);

    cobalt_lfskiplist_node_t *preds[COBALT_LFSKIPLIST_MAX_LEVEL];
    cobalt_lfskiplist_node_t *succs[COBALT_LFSKIPLIST_MAX_LEVEL];

    while (1) {
        if (find_preds_succs(sl, key, preds, succs)) {
            /* Key exists: atomically replace value */
            cobalt_lfskiplist_node_t *victim = succs[0];
            void *old = atomic_exchange_explicit(&victim->value, value, memory_order_acq_rel);
            if (out_old_value) {
                *out_old_value = old;
            }
            cobalt_ebr_exit(h);
            return 1; /* Replaced existing key */
        }

        /* Key does not exist: perform insertion */
        uint8_t                   top_level = random_level();
        cobalt_lfskiplist_node_t *new_node  = node_create(sl, key, value, top_level);
        if (!new_node) {
            cobalt_ebr_exit(h);
            return -1;
        }

        for (uint8_t i = 0; i < top_level; i++) {
            atomic_store_explicit(&new_node->next[i], (uintptr_t)succs[i], memory_order_relaxed);
        }

        uintptr_t expected = (uintptr_t)succs[0];
        uintptr_t desired  = (uintptr_t)new_node;

        if (!atomic_compare_exchange_strong_explicit(&preds[0]->next[0],
                                                     &expected,
                                                     desired,
                                                     memory_order_release,
                                                     memory_order_relaxed)) {
            sl_free(sl, new_node);
            continue;
        }

        /* Link upper levels */
        for (uint8_t i = 1; i < top_level; i++) {
            while (1) {
                uintptr_t my_next = atomic_load_explicit(&new_node->next[i], memory_order_acquire);
                if (COBALT_IS_MARKED(my_next)) {
                    break;
                }

                uintptr_t exp = (uintptr_t)succs[i];
                uintptr_t des = (uintptr_t)new_node;

                if (atomic_compare_exchange_strong_explicit(&preds[i]->next[i],
                                                            &exp,
                                                            des,
                                                            memory_order_release,
                                                            memory_order_relaxed)) {
                    break;
                }

                find_preds_succs(sl, key, preds, succs);
                atomic_store_explicit(
                    &new_node->next[i], (uintptr_t)succs[i], memory_order_relaxed);
            }
        }

        atomic_fetch_add_explicit(&sl->size, 1, memory_order_relaxed);
        if (out_old_value) {
            *out_old_value = NULL;
        }
        cobalt_ebr_exit(h);
        return 0; /* Inserted new key */
    }
}

int cobalt_lfskiplist_get(cobalt_lfskiplist_t *sl, const void *key, void **out_value)
{
    if (!sl || !key) {
        return -1;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(sl);
    cobalt_ebr_enter(h);

    cobalt_lfskiplist_node_t *pred = sl->head;
    cobalt_lfskiplist_node_t *curr = NULL;

    for (int level = COBALT_LFSKIPLIST_MAX_LEVEL - 1; level >= 0; level--) {
        uintptr_t curr_tagged = atomic_load_explicit(&pred->next[level], memory_order_acquire);
        curr                  = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(curr_tagged);

        while (curr != NULL) {
            uintptr_t succ_tagged = atomic_load_explicit(&curr->next[level], memory_order_acquire);

            /* Skip marked nodes without modifying the list */
            while (COBALT_IS_MARKED(succ_tagged)) {
                curr = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(succ_tagged);
                if (curr == NULL) {
                    break;
                }
                succ_tagged = atomic_load_explicit(&curr->next[level], memory_order_acquire);
            }

            if (curr == NULL) {
                break;
            }

            int cmp = sl->compar(curr->key, key);
            if (cmp < 0) {
                pred = curr;
                curr = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(succ_tagged);
            } else if (cmp == 0) {
                uintptr_t l0_next = atomic_load_explicit(&curr->next[0], memory_order_acquire);
                if (!COBALT_IS_MARKED(l0_next)) {
                    if (out_value) {
                        *out_value = atomic_load_explicit(&curr->value, memory_order_acquire);
                    }
                    cobalt_ebr_exit(h);
                    return 0;
                }
                break;
            } else {
                break;
            }
        }
    }

    cobalt_ebr_exit(h);
    return -1;
}

bool cobalt_lfskiplist_contains(cobalt_lfskiplist_t *sl, const void *key)
{
    return (cobalt_lfskiplist_get(sl, key, NULL) == 0);
}

int cobalt_lfskiplist_remove(cobalt_lfskiplist_t *sl, const void *key, void **out_old_value)
{
    if (!sl || !key) {
        return -1;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(sl);
    cobalt_ebr_enter(h);

    cobalt_lfskiplist_node_t *preds[COBALT_LFSKIPLIST_MAX_LEVEL];
    cobalt_lfskiplist_node_t *succs[COBALT_LFSKIPLIST_MAX_LEVEL];

    while (1) {
        if (!find_preds_succs(sl, key, preds, succs)) {
            cobalt_ebr_exit(h);
            return -1; /* Key not found */
        }

        cobalt_lfskiplist_node_t *victim = succs[0];

        /* Phase 1: Mark upper levels top_level - 1 down to 1 */
        for (int i = victim->top_level - 1; i >= 1; i--) {
            uintptr_t next_tagged = atomic_load_explicit(&victim->next[i], memory_order_acquire);
            while (!COBALT_IS_MARKED(next_tagged)) {
                uintptr_t marked = (uintptr_t)COBALT_GET_MARKED(next_tagged);
                if (atomic_compare_exchange_weak_explicit(&victim->next[i],
                                                          &next_tagged,
                                                          marked,
                                                          memory_order_acq_rel,
                                                          memory_order_acquire)) {
                    break;
                }
            }
        }

        /* Linearization point: Mark level 0 */
        uintptr_t l0_next = atomic_load_explicit(&victim->next[0], memory_order_acquire);
        while (1) {
            if (COBALT_IS_MARKED(l0_next)) {
                cobalt_ebr_exit(h);
                return -1; /* Another thread already marked it */
            }

            uintptr_t marked_l0 = (uintptr_t)COBALT_GET_MARKED(l0_next);
            if (atomic_compare_exchange_strong_explicit(&victim->next[0],
                                                        &l0_next,
                                                        marked_l0,
                                                        memory_order_acq_rel,
                                                        memory_order_acquire)) {
                break;
            }
        }

        /* Phase 2: Physical unlinking & retirement */
        if (out_old_value) {
            *out_old_value = atomic_load_explicit(&victim->value, memory_order_acquire);
        }

        /* Trigger find to cooperatively unlink victim across levels */
        find_preds_succs(sl, key, preds, succs);

        /* Retire victim node to EBR */
        cobalt_ebr_retire(h, victim, node_ebr_free_fn, sl);
        atomic_fetch_sub_explicit(&sl->size, 1, memory_order_relaxed);

        cobalt_ebr_exit(h);
        return 0;
    }
}

size_t cobalt_lfskiplist_size(const cobalt_lfskiplist_t *sl)
{
    if (!sl) {
        return 0;
    }
    return atomic_load_explicit(&sl->size, memory_order_relaxed);
}

bool cobalt_lfskiplist_is_empty(const cobalt_lfskiplist_t *sl)
{
    return (cobalt_lfskiplist_size(sl) == 0);
}

cobalt_lfskiplist_iter_t *
cobalt_lfskiplist_iter_create(cobalt_lfskiplist_t *sl, const void *min_key, const void *max_key)
{
    if (!sl) {
        return NULL;
    }

    cobalt_lfskiplist_iter_t *iter =
        (cobalt_lfskiplist_iter_t *)sl_alloc(sl, sizeof(cobalt_lfskiplist_iter_t));
    if (!iter) {
        return NULL;
    }

    iter->skiplist = sl;
    iter->handle   = get_tls_ebr_handle(sl);
    iter->max_key  = max_key;
    iter->finished = false;

    /* Pin EBR epoch for the entire duration of the iterator */
    cobalt_ebr_enter(iter->handle);

    if (min_key) {
        cobalt_lfskiplist_node_t *preds[COBALT_LFSKIPLIST_MAX_LEVEL];
        cobalt_lfskiplist_node_t *succs[COBALT_LFSKIPLIST_MAX_LEVEL];
        find_preds_succs(sl, min_key, preds, succs);
        iter->curr = succs[0];
    } else {
        uintptr_t head_next = atomic_load_explicit(&sl->head->next[0], memory_order_acquire);
        iter->curr          = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(head_next);
    }

    return iter;
}

bool cobalt_lfskiplist_iter_next(cobalt_lfskiplist_iter_t *iter, void **out_key, void **out_value)
{
    if (!iter || iter->finished) {
        return false;
    }

    cobalt_lfskiplist_t *sl = iter->skiplist;

    while (iter->curr != NULL) {
        cobalt_lfskiplist_node_t *n = iter->curr;
        uintptr_t next_tagged       = atomic_load_explicit(&n->next[0], memory_order_acquire);
        iter->curr                  = (cobalt_lfskiplist_node_t *)COBALT_GET_UNMARKED(next_tagged);

        /* Skip marked (logically deleted) nodes */
        if (COBALT_IS_MARKED(next_tagged)) {
            continue;
        }

        /* Check upper bound */
        if (iter->max_key && sl->compar(n->key, iter->max_key) > 0) {
            iter->finished = true;
            return false;
        }

        if (out_key) {
            *out_key = n->key;
        }
        if (out_value) {
            *out_value = atomic_load_explicit(&n->value, memory_order_acquire);
        }
        return true;
    }

    iter->finished = true;
    return false;
}

void cobalt_lfskiplist_iter_destroy(cobalt_lfskiplist_iter_t *iter)
{
    if (!iter) {
        return;
    }
    cobalt_ebr_exit(iter->handle);
    sl_free(iter->skiplist, iter);
}

cobalt_lfskiplist_guard_t cobalt_lfskiplist_pin(cobalt_lfskiplist_t *sl)
{
    cobalt_lfskiplist_guard_t g = {0};
    if (sl) {
        g.handle = get_tls_ebr_handle(sl);
        cobalt_ebr_enter(g.handle);
    }
    return g;
}

void cobalt_lfskiplist_unpin(cobalt_lfskiplist_guard_t *guard)
{
    if (guard && guard->handle) {
        cobalt_ebr_exit(guard->handle);
        guard->handle = NULL;
    }
}
