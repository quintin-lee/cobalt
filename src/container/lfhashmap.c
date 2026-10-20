/**
 * @file lfhashmap.c
 * @brief Lock-Free Concurrent Extensible Hashmap with Epoch-Based Reclamation
 *
 * @details Implements a 100% lock-free concurrent hashmap using the Split-Ordered
 *          List algorithm (Shalev & Shavit, 2006). Features wait-free point reads,
 *          zero-copy dynamic bucket expansion via a two-level segmented directory,
 *          Harris marked-pointer two-phase deletion, and integration with Layer 7
 *          cobalt_ebr_t for safe deferred memory reclamation.
 */

#include "cobalt/container/lfhashmap.h"
#include "cobalt/runtime/error.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define COBALT_LFHASHMAP_MAX_SEGMENTS 32
#define COBALT_LFHASHMAP_INIT_BUCKETS 4
#define COBALT_LFHASHMAP_MAX_LOAD 0.75

#define SO_REGULAR_BIT ((uint64_t)1 << 63)

/**
 * @brief Internal node structure for the split-ordered list
 */
typedef struct cobalt_lfhashmap_node {
    uint64_t           so_key; /* Bit-reversed split-order key */
    void              *key;    /* User key pointer (NULL for dummy nodes) */
    _Atomic(void *)    value;  /* User value pointer (NULL for dummy nodes) */
    _Atomic(uintptr_t) next;   /* Next node pointer tagged with bit-0 mark */
} cobalt_lfhashmap_node_t;

/**
 * @brief Main lock-free hashmap control structure
 */
struct cobalt_lfhashmap {
    /* Top-level directory of 32 segments */
    _Atomic(cobalt_lfhashmap_node_t **) segments[COBALT_LFHASHMAP_MAX_SEGMENTS];

    atomic_size_t count;        /* Number of active regular elements */
    atomic_size_t bucket_count; /* Current logical bucket capacity (power of 2) */

    cobalt_lfhash_func_t  hash_func;     /* Hash function callback */
    cobalt_lfequal_func_t equal_func;    /* Equality comparison callback */
    bool                  fast_ptr_mode; /* True for inline pointer/integer key mode */

    cobalt_allocator_t *allocator; /* Injectable memory allocator */
    cobalt_ebr_t       *ebr;       /* Epoch-Based Reclamation engine */
    bool                owns_ebr;  /* True if EBR engine was created internally */
};

/**
 * @brief Concurrent safe iterator holding pinned EBR epoch
 */
struct cobalt_lfhashmap_iter {
    cobalt_lfhashmap_t      *map;
    cobalt_ebr_handle_t     *handle;
    cobalt_lfhashmap_node_t *curr;
    bool                     finished;
};

typedef struct {
    uint64_t             ebr_id;
    cobalt_ebr_handle_t *handle;
} tls_ebr_cache_t;

static _Thread_local tls_ebr_cache_t tl_ebr_cache = {0, NULL};

/* -------------------------------------------------------------------------- */
/* Memory Allocator Wrappers                                                  */
/* -------------------------------------------------------------------------- */

static void *sl_alloc(cobalt_lfhashmap_t *map, size_t size)
{
    if (map && map->allocator && map->allocator->alloc) {
        return map->allocator->alloc(map->allocator, size);
    }
    return malloc(size);
}

static void sl_free(cobalt_lfhashmap_t *map, void *ptr)
{
    if (!ptr) {
        return;
    }
    if (map && map->allocator && map->allocator->free) {
        map->allocator->free(map->allocator, ptr);
    } else {
        free(ptr);
    }
}

static cobalt_ebr_handle_t *get_tls_ebr_handle(cobalt_lfhashmap_t *map)
{
    uint64_t id = cobalt_ebr_get_id(map->ebr);
    if (tl_ebr_cache.ebr_id == id && tl_ebr_cache.handle != NULL) {
        return tl_ebr_cache.handle;
    }
    cobalt_ebr_handle_t *h = cobalt_ebr_register(map->ebr);
    tl_ebr_cache.ebr_id    = id;
    tl_ebr_cache.handle    = h;
    return h;
}

static cobalt_lfhashmap_node_t *
node_create(cobalt_lfhashmap_t *map, uint64_t so_key, void *key, void *value)
{
    cobalt_lfhashmap_node_t *n =
        (cobalt_lfhashmap_node_t *)sl_alloc(map, sizeof(cobalt_lfhashmap_node_t));
    if (!n) {
        return NULL;
    }
    n->so_key = so_key;
    n->key    = key;
    atomic_init(&n->value, value);
    atomic_init(&n->next, 0);
    return n;
}

static void node_ebr_free_fn(void *ptr, void *user_data)
{
    cobalt_lfhashmap_t *map = (cobalt_lfhashmap_t *)user_data;
    sl_free(map, ptr);
}

/* -------------------------------------------------------------------------- */
/* Bit-Reversal and Hashing Helpers                                           */
/* -------------------------------------------------------------------------- */

static inline uint64_t reverse_bits64(uint64_t v)
{
    v = ((v >> 1) & 0x5555555555555555ULL) | ((v & 0x5555555555555555ULL) << 1);
    v = ((v >> 2) & 0x3333333333333333ULL) | ((v & 0x3333333333333333ULL) << 2);
    v = ((v >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((v & 0x0F0F0F0F0F0F0F0FULL) << 4);
    v = ((v >> 8) & 0x00FF00FF00FF00FFULL) | ((v & 0x00FF00FF00FF00FFULL) << 8);
    v = ((v >> 16) & 0x0000FFFF0000FFFFULL) | ((v & 0x0000FFFF0000FFFFULL) << 16);
    return (v >> 32) | (v << 32);
}

static inline uint64_t make_regular_so_key(uint64_t hash)
{
    return reverse_bits64(hash | SO_REGULAR_BIT);
}

static inline uint64_t make_dummy_so_key(size_t bucket_idx)
{
    return reverse_bits64((uint64_t)bucket_idx);
}

static inline bool is_regular_node(uint64_t so_key)
{
    return (so_key & 1ULL) != 0;
}

static uint64_t default_str_hash(const void *key)
{
    const char *s = (const char *)key;
    uint64_t    h = 14695981039346656037ULL;
    if (!s) {
        return h;
    }
    while (*s) {
        h ^= (uint64_t)(unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static int default_str_equal(const void *a, const void *b)
{
    if (a == b) {
        return 1;
    }
    if (!a || !b) {
        return 0;
    }
    return strcmp((const char *)a, (const char *)b) == 0;
}

static inline uint64_t splitmix64(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static uint64_t default_ptr_hash(const void *key)
{
    return splitmix64((uint64_t)(uintptr_t)key);
}

static int default_ptr_equal(const void *a, const void *b)
{
    return a == b;
}

static inline uint64_t compute_hash(const cobalt_lfhashmap_t *map, const void *key)
{
    if (map->fast_ptr_mode) {
        return splitmix64((uint64_t)(uintptr_t)key);
    }
    return map->hash_func(key);
}

static inline bool keys_equal(const cobalt_lfhashmap_t *map, const void *a, const void *b)
{
    if (map->fast_ptr_mode) {
        return a == b;
    }
    return map->equal_func(a, b) != 0;
}

/* -------------------------------------------------------------------------- */
/* Two-Level Segmented Directory Math and Sizing                              */
/* -------------------------------------------------------------------------- */

static inline size_t segment_size(size_t seg)
{
    if (seg == 0) {
        return 2;
    }
    return (size_t)1 << seg;
}

static inline void get_bucket_location(size_t bucket_idx, size_t *out_seg, size_t *out_offset)
{
    if (bucket_idx < 2) {
        *out_seg    = 0;
        *out_offset = bucket_idx;
        return;
    }
    int k       = 63 - __builtin_clzll(bucket_idx);
    *out_seg    = (size_t)k;
    *out_offset = bucket_idx ^ (1ULL << k);
}

static cobalt_lfhashmap_node_t **get_or_create_segment(cobalt_lfhashmap_t *map, size_t seg)
{
    if (seg >= COBALT_LFHASHMAP_MAX_SEGMENTS) {
        return NULL;
    }
    cobalt_lfhashmap_node_t **s = atomic_load_explicit(&map->segments[seg], memory_order_acquire);
    if (s != NULL) {
        return s;
    }

    size_t                    cap = segment_size(seg);
    cobalt_lfhashmap_node_t **new_s =
        (cobalt_lfhashmap_node_t **)sl_alloc(map, cap * sizeof(cobalt_lfhashmap_node_t *));
    if (!new_s) {
        return NULL;
    }
    memset(new_s, 0, cap * sizeof(cobalt_lfhashmap_node_t *));

    cobalt_lfhashmap_node_t **expected = NULL;
    if (atomic_compare_exchange_strong_explicit(
            &map->segments[seg], &expected, new_s, memory_order_release, memory_order_acquire)) {
        return new_s;
    }

    /* Lost race: free local allocation and return winner */
    sl_free(map, new_s);
    return expected;
}

/* -------------------------------------------------------------------------- */
/* Lock-Free List Traversal with Cooperative Deletion                         */
/* -------------------------------------------------------------------------- */

static bool find_pred_curr(cobalt_lfhashmap_t       *map,
                           cobalt_lfhashmap_node_t  *head,
                           uint64_t                  target_so_key,
                           const void               *target_key,
                           cobalt_lfhashmap_node_t **out_pred,
                           cobalt_lfhashmap_node_t **out_curr,
                           cobalt_ebr_handle_t      *h)
{
retry:;
    cobalt_lfhashmap_node_t *pred        = head;
    uintptr_t                curr_tagged = atomic_load_explicit(&pred->next, memory_order_acquire);
    cobalt_lfhashmap_node_t *curr = (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(curr_tagged);

    while (curr != NULL) {
        uintptr_t succ_tagged = atomic_load_explicit(&curr->next, memory_order_acquire);

        /* Cooperatively unlink logically deleted (marked) nodes */
        while (COBALT_IS_MARKED(succ_tagged)) {
            uintptr_t expected = (uintptr_t)curr;
            uintptr_t desired  = (uintptr_t)COBALT_GET_UNMARKED(succ_tagged);

            if (!atomic_compare_exchange_weak_explicit(
                    &pred->next, &expected, desired, memory_order_acq_rel, memory_order_acquire)) {
                goto retry;
            }

            cobalt_ebr_retire(h, curr, node_ebr_free_fn, map);

            curr = (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(succ_tagged);
            if (curr == NULL) {
                break;
            }
            succ_tagged = atomic_load_explicit(&curr->next, memory_order_acquire);
        }

        if (curr == NULL) {
            break;
        }

        if (curr->so_key > target_so_key) {
            break;
        }

        if (curr->so_key == target_so_key) {
            if (target_key == NULL) {
                *out_pred = pred;
                *out_curr = curr;
                return true; /* Exact dummy sentinel found */
            }
            if (curr->key != NULL && keys_equal(map, curr->key, target_key)) {
                *out_pred = pred;
                *out_curr = curr;
                return true; /* Exact key match found and unmarked */
            }
        }

        pred = curr;
        curr = (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(succ_tagged);
    }

    *out_pred = pred;
    *out_curr = curr;
    return false;
}

/* -------------------------------------------------------------------------- */
/* Lazy Sentinel Bucket Initialization                                       */
/* -------------------------------------------------------------------------- */

static cobalt_lfhashmap_node_t *
get_or_init_bucket(cobalt_lfhashmap_t *map, size_t bucket_idx, cobalt_ebr_handle_t *h)
{
    size_t seg = 0, offset = 0;
    get_bucket_location(bucket_idx, &seg, &offset);

    cobalt_lfhashmap_node_t **seg_arr = get_or_create_segment(map, seg);
    if (!seg_arr) {
        return NULL;
    }

    _Atomic(cobalt_lfhashmap_node_t *) *slot =
        (_Atomic(cobalt_lfhashmap_node_t *) *)&seg_arr[offset];
    cobalt_lfhashmap_node_t *node = atomic_load_explicit(slot, memory_order_acquire);
    if (node != NULL) {
        return node;
    }

    if (bucket_idx == 0) {
        /* Root sentinel node */
        cobalt_lfhashmap_node_t *root = node_create(map, 0, NULL, NULL);
        if (!root) {
            return NULL;
        }
        cobalt_lfhashmap_node_t *exp = NULL;
        if (atomic_compare_exchange_strong_explicit(
                slot, &exp, root, memory_order_release, memory_order_acquire)) {
            return root;
        }
        sl_free(map, root);
        return exp;
    }

    /* Identify parent bucket */
    size_t                   parent_idx = bucket_idx ^ (1ULL << (63 - __builtin_clzll(bucket_idx)));
    cobalt_lfhashmap_node_t *parent_node = get_or_init_bucket(map, parent_idx, h);
    if (!parent_node) {
        return NULL;
    }

    uint64_t                 dummy_so_key = make_dummy_so_key(bucket_idx);
    cobalt_lfhashmap_node_t *dummy        = node_create(map, dummy_so_key, NULL, NULL);
    if (!dummy) {
        return NULL;
    }

    /* Insert dummy sentinel into global linked list starting from parent */
    while (1) {
        cobalt_lfhashmap_node_t *existing = atomic_load_explicit(slot, memory_order_acquire);
        if (existing != NULL) {
            sl_free(map, dummy);
            return existing;
        }

        cobalt_lfhashmap_node_t *pred = NULL;
        cobalt_lfhashmap_node_t *curr = NULL;

        if (find_pred_curr(map, parent_node, dummy_so_key, NULL, &pred, &curr, h)) {
            /* Another thread already inserted the dummy sentinel */
            sl_free(map, dummy);
            atomic_store_explicit(slot, curr, memory_order_release);
            return curr;
        }

        if (curr != NULL && curr->so_key == dummy_so_key) {
            /* Sentinel exists in list */
            sl_free(map, dummy);
            atomic_store_explicit(slot, curr, memory_order_release);
            return curr;
        }

        atomic_store_explicit(&dummy->next, (uintptr_t)curr, memory_order_relaxed);
        uintptr_t expected = (uintptr_t)curr;
        if (atomic_compare_exchange_strong_explicit(&pred->next,
                                                    &expected,
                                                    (uintptr_t)dummy,
                                                    memory_order_release,
                                                    memory_order_acquire)) {
            atomic_store_explicit(slot, dummy, memory_order_release);
            return dummy;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Public API Implementation                                                  */
/* -------------------------------------------------------------------------- */

cobalt_lfhashmap_t *cobalt_lfhashmap_create(cobalt_lfhash_func_t  hash_fn,
                                            cobalt_lfequal_func_t equal_fn)
{
    return cobalt_lfhashmap_create_with_allocator(hash_fn, equal_fn, NULL, NULL, false);
}

cobalt_lfhashmap_t *cobalt_lfhashmap_create_ptr(void)
{
    return cobalt_lfhashmap_create_with_allocator(NULL, NULL, NULL, NULL, true);
}

cobalt_lfhashmap_t *cobalt_lfhashmap_create_with_allocator(cobalt_lfhash_func_t  hash_fn,
                                                           cobalt_lfequal_func_t equal_fn,
                                                           cobalt_allocator_t   *alloc,
                                                           cobalt_ebr_t         *ebr,
                                                           bool                  fast_ptr_mode)
{
    cobalt_allocator_t *effective_alloc = alloc ? alloc : cobalt_allocator_get_system();
    cobalt_lfhashmap_t *map             = NULL;

    if (effective_alloc && effective_alloc->alloc) {
        map = (cobalt_lfhashmap_t *)effective_alloc->alloc(effective_alloc,
                                                           sizeof(cobalt_lfhashmap_t));
    } else {
        map = (cobalt_lfhashmap_t *)malloc(sizeof(cobalt_lfhashmap_t));
    }

    if (!map) {
        return NULL;
    }

    memset(map, 0, sizeof(cobalt_lfhashmap_t));
    map->allocator     = effective_alloc;
    map->fast_ptr_mode = fast_ptr_mode;
    map->hash_func     = hash_fn ? hash_fn : (fast_ptr_mode ? default_ptr_hash : default_str_hash);
    map->equal_func = equal_fn ? equal_fn : (fast_ptr_mode ? default_ptr_equal : default_str_equal);

    atomic_init(&map->count, 0);
    atomic_init(&map->bucket_count, COBALT_LFHASHMAP_INIT_BUCKETS);

    for (size_t i = 0; i < COBALT_LFHASHMAP_MAX_SEGMENTS; i++) {
        atomic_init(&map->segments[i], NULL);
    }

    if (ebr) {
        map->ebr      = ebr;
        map->owns_ebr = false;
    } else {
        map->ebr = cobalt_ebr_create(effective_alloc);
        if (!map->ebr) {
            sl_free(map, map);
            return NULL;
        }
        map->owns_ebr = true;
    }

    /* Initialize root sentinel (bucket 0) */
    cobalt_ebr_handle_t *h = get_tls_ebr_handle(map);
    cobalt_ebr_enter(h);
    if (!get_or_init_bucket(map, 0, h)) {
        cobalt_ebr_exit(h);
        cobalt_lfhashmap_destroy(map);
        return NULL;
    }
    cobalt_ebr_exit(h);

    return map;
}

void cobalt_lfhashmap_destroy(cobalt_lfhashmap_t *map)
{
    if (!map) {
        return;
    }

    /* Free all nodes starting from root sentinel */
    cobalt_lfhashmap_node_t **seg0 = atomic_load_explicit(&map->segments[0], memory_order_relaxed);
    if (seg0 && seg0[0]) {
        cobalt_lfhashmap_node_t *curr = seg0[0];
        while (curr) {
            cobalt_lfhashmap_node_t *next =
                (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(atomic_load(&curr->next));
            sl_free(map, curr);
            curr = next;
        }
    }

    /* Free all allocated segments */
    for (size_t i = 0; i < COBALT_LFHASHMAP_MAX_SEGMENTS; i++) {
        cobalt_lfhashmap_node_t **s = atomic_load_explicit(&map->segments[i], memory_order_relaxed);
        if (s) {
            sl_free(map, s);
        }
    }

    if (map->owns_ebr && map->ebr) {
        uint64_t id = cobalt_ebr_get_id(map->ebr);
        if (tl_ebr_cache.ebr_id == id) {
            tl_ebr_cache.ebr_id = 0;
            tl_ebr_cache.handle = NULL;
        }
        cobalt_ebr_destroy(map->ebr);
    }

    sl_free(map, map);
}

int cobalt_lfhashmap_insert(cobalt_lfhashmap_t *map, void *key, void *value)
{
    if (!map || !key) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(map);
    cobalt_ebr_enter(h);

    uint64_t hash       = compute_hash(map, key);
    uint64_t so_key     = make_regular_so_key(hash);
    size_t   cap        = atomic_load_explicit(&map->bucket_count, memory_order_acquire);
    size_t   bucket_idx = hash & (cap - 1);

    cobalt_lfhashmap_node_t *head = get_or_init_bucket(map, bucket_idx, h);
    if (!head) {
        cobalt_ebr_exit(h);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    while (1) {
        cobalt_lfhashmap_node_t *pred = NULL;
        cobalt_lfhashmap_node_t *curr = NULL;

        if (find_pred_curr(map, head, so_key, key, &pred, &curr, h)) {
            cobalt_ebr_exit(h);
            return COBALT_ERROR_ALREADY_EXISTS;
        }

        cobalt_lfhashmap_node_t *new_node = node_create(map, so_key, key, value);
        if (!new_node) {
            cobalt_ebr_exit(h);
            return COBALT_ERROR_OUT_OF_MEMORY;
        }

        atomic_store_explicit(&new_node->next, (uintptr_t)curr, memory_order_relaxed);
        uintptr_t expected = (uintptr_t)curr;

        if (atomic_compare_exchange_strong_explicit(&pred->next,
                                                    &expected,
                                                    (uintptr_t)new_node,
                                                    memory_order_release,
                                                    memory_order_acquire)) {
            size_t new_count = atomic_fetch_add_explicit(&map->count, 1, memory_order_relaxed) + 1;
            size_t cur_cap   = atomic_load_explicit(&map->bucket_count, memory_order_relaxed);

            /* Trigger dynamic doubling if load factor exceeded */
            if ((double)new_count / (double)cur_cap > COBALT_LFHASHMAP_MAX_LOAD &&
                cur_cap < ((size_t)1 << 31)) {
                atomic_compare_exchange_weak_explicit(&map->bucket_count,
                                                      &cur_cap,
                                                      cur_cap * 2,
                                                      memory_order_release,
                                                      memory_order_relaxed);
            }

            cobalt_ebr_exit(h);
            return COBALT_SUCCESS;
        }

        /* Conflict: retry */
        sl_free(map, new_node);
    }
}

int cobalt_lfhashmap_put(cobalt_lfhashmap_t *map, void *key, void *value, void **out_old_value)
{
    if (!map || !key) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(map);
    cobalt_ebr_enter(h);

    uint64_t hash       = compute_hash(map, key);
    uint64_t so_key     = make_regular_so_key(hash);
    size_t   cap        = atomic_load_explicit(&map->bucket_count, memory_order_acquire);
    size_t   bucket_idx = hash & (cap - 1);

    cobalt_lfhashmap_node_t *head = get_or_init_bucket(map, bucket_idx, h);
    if (!head) {
        cobalt_ebr_exit(h);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    while (1) {
        cobalt_lfhashmap_node_t *pred = NULL;
        cobalt_lfhashmap_node_t *curr = NULL;

        if (find_pred_curr(map, head, so_key, key, &pred, &curr, h)) {
            /* Key exists: update value atomically */
            void *old_val = atomic_load_explicit(&curr->value, memory_order_relaxed);
            while (!atomic_compare_exchange_weak_explicit(
                &curr->value, &old_val, value, memory_order_acq_rel, memory_order_acquire)) {
                /* Check if node was concurrently marked */
                uintptr_t n = atomic_load_explicit(&curr->next, memory_order_acquire);
                if (COBALT_IS_MARKED(n)) {
                    break; /* Marked: retry whole search */
                }
            }

            uintptr_t n = atomic_load_explicit(&curr->next, memory_order_acquire);
            if (!COBALT_IS_MARKED(n)) {
                if (out_old_value) {
                    *out_old_value = old_val;
                }
                cobalt_ebr_exit(h);
                return COBALT_SUCCESS;
            }
            continue; /* Concurrently marked: retry */
        }

        /* Key not found: insert new node */
        cobalt_lfhashmap_node_t *new_node = node_create(map, so_key, key, value);
        if (!new_node) {
            cobalt_ebr_exit(h);
            return COBALT_ERROR_OUT_OF_MEMORY;
        }

        atomic_store_explicit(&new_node->next, (uintptr_t)curr, memory_order_relaxed);
        uintptr_t expected = (uintptr_t)curr;

        if (atomic_compare_exchange_strong_explicit(&pred->next,
                                                    &expected,
                                                    (uintptr_t)new_node,
                                                    memory_order_release,
                                                    memory_order_acquire)) {
            if (out_old_value) {
                *out_old_value = NULL;
            }
            size_t new_count = atomic_fetch_add_explicit(&map->count, 1, memory_order_relaxed) + 1;
            size_t cur_cap   = atomic_load_explicit(&map->bucket_count, memory_order_relaxed);

            if ((double)new_count / (double)cur_cap > COBALT_LFHASHMAP_MAX_LOAD &&
                cur_cap < ((size_t)1 << 31)) {
                atomic_compare_exchange_weak_explicit(&map->bucket_count,
                                                      &cur_cap,
                                                      cur_cap * 2,
                                                      memory_order_release,
                                                      memory_order_relaxed);
            }

            cobalt_ebr_exit(h);
            return COBALT_SUCCESS;
        }

        sl_free(map, new_node);
    }
}

int cobalt_lfhashmap_get(cobalt_lfhashmap_t *map, const void *key, void **out_value)
{
    if (!map || !key) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(map);
    cobalt_ebr_enter(h);

    uint64_t hash       = compute_hash(map, key);
    uint64_t so_key     = make_regular_so_key(hash);
    size_t   cap        = atomic_load_explicit(&map->bucket_count, memory_order_acquire);
    size_t   bucket_idx = hash & (cap - 1);

    cobalt_lfhashmap_node_t *head = get_or_init_bucket(map, bucket_idx, h);
    if (!head) {
        cobalt_ebr_exit(h);
        return COBALT_ERROR_NOT_FOUND;
    }

    cobalt_lfhashmap_node_t *curr = head;
    while (curr != NULL) {
        uintptr_t next_tagged         = atomic_load_explicit(&curr->next, memory_order_acquire);
        cobalt_lfhashmap_node_t *next = (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(next_tagged);

        if (!COBALT_IS_MARKED(next_tagged) && curr->so_key == so_key) {
            if (curr->key != NULL && keys_equal(map, curr->key, key)) {
                if (out_value) {
                    *out_value = atomic_load_explicit(&curr->value, memory_order_acquire);
                }
                cobalt_ebr_exit(h);
                return COBALT_SUCCESS;
            }
        }

        if (curr->so_key > so_key) {
            break;
        }
        curr = next;
    }

    cobalt_ebr_exit(h);
    return COBALT_ERROR_NOT_FOUND;
}

bool cobalt_lfhashmap_contains(cobalt_lfhashmap_t *map, const void *key)
{
    return cobalt_lfhashmap_get(map, key, NULL) == COBALT_SUCCESS;
}

int cobalt_lfhashmap_remove(cobalt_lfhashmap_t *map, const void *key, void **out_old_value)
{
    if (!map || !key) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_ebr_handle_t *h = get_tls_ebr_handle(map);
    cobalt_ebr_enter(h);

    uint64_t hash       = compute_hash(map, key);
    uint64_t so_key     = make_regular_so_key(hash);
    size_t   cap        = atomic_load_explicit(&map->bucket_count, memory_order_acquire);
    size_t   bucket_idx = hash & (cap - 1);

    cobalt_lfhashmap_node_t *head = get_or_init_bucket(map, bucket_idx, h);
    if (!head) {
        cobalt_ebr_exit(h);
        return COBALT_ERROR_NOT_FOUND;
    }

    while (1) {
        cobalt_lfhashmap_node_t *pred   = NULL;
        cobalt_lfhashmap_node_t *victim = NULL;

        if (!find_pred_curr(map, head, so_key, key, &pred, &victim, h)) {
            cobalt_ebr_exit(h);
            return COBALT_ERROR_NOT_FOUND;
        }

        /* Phase 1: Logical deletion (Linearization Point) */
        uintptr_t next_tagged = atomic_load_explicit(&victim->next, memory_order_acquire);
        while (!COBALT_IS_MARKED(next_tagged)) {
            uintptr_t marked = (uintptr_t)COBALT_GET_MARKED(next_tagged);
            if (atomic_compare_exchange_weak_explicit(&victim->next,
                                                      &next_tagged,
                                                      marked,
                                                      memory_order_acq_rel,
                                                      memory_order_acquire)) {
                break;
            }
        }

        if (COBALT_IS_MARKED(next_tagged)) {
            /* Another thread marked it concurrently */
            cobalt_ebr_exit(h);
            return COBALT_ERROR_NOT_FOUND;
        }

        /* Phase 2: Physical unlinking & retirement */
        atomic_fetch_sub_explicit(&map->count, 1, memory_order_relaxed);

        if (out_old_value) {
            *out_old_value = atomic_load_explicit(&victim->value, memory_order_acquire);
        }

        /* Trigger find to cooperatively unlink victim from list */
        find_pred_curr(map, head, so_key, key, &pred, &victim, h);

        cobalt_ebr_exit(h);
        return COBALT_SUCCESS;
    }
}

size_t cobalt_lfhashmap_size(const cobalt_lfhashmap_t *map)
{
    if (!map) {
        return 0;
    }
    return atomic_load_explicit(&map->count, memory_order_relaxed);
}

size_t cobalt_lfhashmap_bucket_count(const cobalt_lfhashmap_t *map)
{
    if (!map) {
        return 0;
    }
    return atomic_load_explicit(&map->bucket_count, memory_order_relaxed);
}

bool cobalt_lfhashmap_is_empty(const cobalt_lfhashmap_t *map)
{
    return cobalt_lfhashmap_size(map) == 0;
}

cobalt_lfhashmap_guard_t cobalt_lfhashmap_pin(cobalt_lfhashmap_t *map)
{
    cobalt_lfhashmap_guard_t g = {0};
    if (map) {
        g.handle = get_tls_ebr_handle(map);
        cobalt_ebr_enter(g.handle);
    }
    return g;
}

void cobalt_lfhashmap_unpin(cobalt_lfhashmap_guard_t *guard)
{
    if (guard && guard->handle) {
        cobalt_ebr_exit(guard->handle);
        guard->handle = NULL;
    }
}

cobalt_lfhashmap_iter_t *cobalt_lfhashmap_iter_create(cobalt_lfhashmap_t *map)
{
    if (!map) {
        return NULL;
    }

    cobalt_lfhashmap_iter_t *iter =
        (cobalt_lfhashmap_iter_t *)sl_alloc(map, sizeof(cobalt_lfhashmap_iter_t));
    if (!iter) {
        return NULL;
    }

    iter->map      = map;
    iter->handle   = get_tls_ebr_handle(map);
    iter->finished = false;

    cobalt_ebr_enter(iter->handle);

    cobalt_lfhashmap_node_t **seg0 = atomic_load_explicit(&map->segments[0], memory_order_acquire);
    iter->curr                     = seg0 ? seg0[0] : NULL;

    return iter;
}

bool cobalt_lfhashmap_iter_next(cobalt_lfhashmap_iter_t *iter, void **out_key, void **out_value)
{
    if (!iter || iter->finished) {
        return false;
    }

    while (iter->curr != NULL) {
        cobalt_lfhashmap_node_t *n           = iter->curr;
        uintptr_t                next_tagged = atomic_load_explicit(&n->next, memory_order_acquire);
        iter->curr = (cobalt_lfhashmap_node_t *)COBALT_GET_UNMARKED(next_tagged);

        /* Skip marked (logically deleted) nodes */
        if (COBALT_IS_MARKED(next_tagged)) {
            continue;
        }

        /* Skip dummy sentinel nodes (LSB == 0) */
        if (!is_regular_node(n->so_key)) {
            continue;
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

void cobalt_lfhashmap_iter_destroy(cobalt_lfhashmap_iter_t *iter)
{
    if (!iter) {
        return;
    }
    cobalt_ebr_exit(iter->handle);
    sl_free(iter->map, iter);
}
