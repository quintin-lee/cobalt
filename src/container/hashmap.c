/**
 * @file hashmap.c
 * @brief Hash map container implemented with Robin Hood open addressing
 *
 * @details Replaces separate chaining with a contiguous flat array of slots using
 *          Robin Hood hashing (rich steal from the poor) for minimal PSL variance,
 *          backward-shift tombstone-free deletion, and fast early-exit lookups.
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* FNV-1a parameters */
enum { COBALT_FNV1A_OFFSET_BASIS = 2166136261U };
enum { COBALT_FNV1A_PRIME = 16777619U };
/* Default slot count for lazy init */
enum { COBALT_HASHMAP_DEFAULT_BUCKETS = 16 };

#define COBALT_HASHMAP_EMPTY_PSL ((uint32_t)0xFFFFFFFF)

/**
 * @brief Round a value up to the next power of two (minimum 2)
 */
static inline size_t hashmap_next_pow2(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#ifdef __LP64__
    v |= v >> 32;
#endif
    v++;
    return v < 2 ? 2 : v;
}

/**
 * @brief Hash a NUL-terminated string with FNV-1a
 */
static unsigned int hash_string(const char *str)
{
    unsigned int hash_val = COBALT_FNV1A_OFFSET_BASIS;
    while (*str) {
        hash_val = (hash_val ^ (unsigned char)*str) * COBALT_FNV1A_PRIME;
        str++;
    }
    return hash_val;
}

/* -------------------------------------------------------------------------- */
/* Internal slot and impl structures                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Single slot in the flat array
 */
typedef struct {
    void    *key;       /**< Key pointer (owned copy if key_owned != 0) */
    void    *value;     /**< Value pointer */
    uint32_t hash;      /**< Cached 32-bit hash digest */
    uint32_t psl;       /**< Probe sequence length (EMPTY_PSL if empty) */
    size_t   key_len;   /**< Key length in bytes */
    uint8_t  key_owned; /**< Non-zero when key memory is owned by map */
} hashmap_slot_t;

/**
 * @brief Internal hashmap state
 */
typedef struct {
    hashmap_slot_t     *slots;      /**< Contiguous slot array (power-of-two capacity) */
    size_t              capacity;   /**< Number of slots */
    size_t              mask;       /**< capacity - 1 */
    size_t              size;       /**< Number of active entries */
    size_t              max_load;   /**< Expansion threshold (capacity * 0.75) */
    cobalt_hash_func_t  hash_func;  /**< Custom hash callback */
    cobalt_equal_func_t equal_func; /**< Custom equality callback */
    cobalt_allocator_t *alloc;      /**< Allocator */
} hashmap_impl_t;

struct cobalt_hashmap {
    cobalt_map_t   base;
    hashmap_impl_t impl;
};

/* Retained for ABI compatibility with forward declarations */
struct cobalt_hashmap_node {
    void *dummy;
};

/* -------------------------------------------------------------------------- */
/* Key hashing and equality helpers                                           */
/* -------------------------------------------------------------------------- */

static inline unsigned int slot_hash(const hashmap_impl_t *impl, const void *key, size_t key_len)
{
    if (impl->hash_func) {
        return impl->hash_func(key, key_len);
    }
    return hash_string((const char *)key);
}

static inline int
slot_equal(const hashmap_impl_t *impl, const hashmap_slot_t *slot, const void *key, size_t key_len)
{
    if (impl->equal_func) {
        return impl->equal_func(key, slot->key, key_len) != 0;
    }
    return strcmp((const char *)key, (const char *)slot->key) == 0;
}

/* -------------------------------------------------------------------------- */
/* Robin Hood Core Algorithms                                                 */
/* -------------------------------------------------------------------------- */

static inline void init_slots(hashmap_slot_t *slots, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        slots[i].key       = NULL;
        slots[i].value     = NULL;
        slots[i].hash      = 0;
        slots[i].psl       = COBALT_HASHMAP_EMPTY_PSL;
        slots[i].key_len   = 0;
        slots[i].key_owned = 0;
    }
}

/**
 * @brief Locate slot matching key and hash, utilizing Robin Hood early exit
 * @return Index of matching slot, or (size_t)-1 if not found
 */
static size_t
find_slot(const hashmap_impl_t *impl, const void *key, size_t key_len, unsigned int hash)
{
    if (!impl->slots || impl->capacity == 0 || impl->size == 0) {
        return (size_t)-1;
    }

    size_t   idx     = (size_t)(hash & impl->mask);
    uint32_t cur_psl = 0;

    for (;;) {
        const hashmap_slot_t *slot = &impl->slots[idx];
        if (slot->psl == COBALT_HASHMAP_EMPTY_PSL) {
            return (size_t)-1;
        }
        if (slot->psl < cur_psl) {
            /* Robin Hood Early Exit */
            return (size_t)-1;
        }
        if (slot->hash == hash && slot_equal(impl, slot, key, key_len)) {
            return idx;
        }
        idx = (idx + 1) & impl->mask;
        cur_psl++;
    }
}

/**
 * @brief Insert an entry using Robin Hood displacement into the slot array
 */
static void robin_hood_insert_entry(hashmap_slot_t *slots, size_t mask, hashmap_slot_t entry)
{
    size_t         idx  = (size_t)(entry.hash & mask);
    hashmap_slot_t curr = entry;
    curr.psl            = 0;

    for (;;) {
        hashmap_slot_t *slot = &slots[idx];
        if (slot->psl == COBALT_HASHMAP_EMPTY_PSL) {
            *slot = curr;
            return;
        }
        if (slot->psl < curr.psl) {
            /* Rich steals from poor: swap */
            hashmap_slot_t tmp = *slot;
            *slot              = curr;
            curr               = tmp;
        }
        idx = (idx + 1) & mask;
        curr.psl++;
    }
}

/**
 * @brief Ensure slot array has at least min_capacity and rehash if growing
 */
static int hashmap_ensure_capacity(hashmap_impl_t *impl, size_t min_capacity)
{
    size_t new_cap = hashmap_next_pow2(min_capacity < COBALT_HASHMAP_DEFAULT_BUCKETS
                                           ? COBALT_HASHMAP_DEFAULT_BUCKETS
                                           : min_capacity);
    if (new_cap <= impl->capacity && impl->slots != NULL) {
        return 0;
    }

    hashmap_slot_t *new_slots =
        (hashmap_slot_t *)impl->alloc->alloc(impl->alloc, sizeof(hashmap_slot_t) * new_cap);
    if (!new_slots) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    init_slots(new_slots, new_cap);

    size_t new_mask = new_cap - 1;
    if (impl->slots && impl->size > 0) {
        for (size_t i = 0; i < impl->capacity; i++) {
            if (impl->slots[i].psl != COBALT_HASHMAP_EMPTY_PSL) {
                robin_hood_insert_entry(new_slots, new_mask, impl->slots[i]);
            }
        }
    }

    if (impl->slots) {
        impl->alloc->free(impl->alloc, impl->slots);
    }

    impl->slots    = new_slots;
    impl->capacity = new_cap;
    impl->mask     = new_mask;
    impl->max_load = (new_cap * 3) / 4;
    return 0;
}

/**
 * @brief Remove key with backward shift deletion (tombstone-free compaction)
 */
static int
hashmap_remove_key(hashmap_impl_t *impl, const void *key, size_t key_len, unsigned int hash)
{
    size_t idx = find_slot(impl, key, key_len, hash);
    if (idx == (size_t)-1) {
        return -1;
    }

    /* Free owned key memory if applicable */
    if (impl->slots[idx].key_owned && impl->slots[idx].key) {
        impl->alloc->free(impl->alloc, impl->slots[idx].key);
    }

    /* Backward shift */
    size_t curr = idx;
    for (;;) {
        size_t next = (curr + 1) & impl->mask;
        if (impl->slots[next].psl == COBALT_HASHMAP_EMPTY_PSL || impl->slots[next].psl == 0) {
            break;
        }
        impl->slots[curr] = impl->slots[next];
        impl->slots[curr].psl--;
        curr = next;
    }

    impl->slots[curr].key       = NULL;
    impl->slots[curr].value     = NULL;
    impl->slots[curr].hash      = 0;
    impl->slots[curr].psl       = COBALT_HASHMAP_EMPTY_PSL;
    impl->slots[curr].key_len   = 0;
    impl->slots[curr].key_owned = 0;
    impl->size--;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Map iterator (hashmap-specific)                                           */
/* -------------------------------------------------------------------------- */

typedef struct {
    hashmap_impl_t     *impl;
    size_t              slot_idx;
    cobalt_allocator_t *alloc;
} hashmap_map_iter_t;

static inline void hashmap_iter_advance(hashmap_map_iter_t *iter)
{
    while (iter->slot_idx < iter->impl->capacity) {
        if (iter->impl->slots[iter->slot_idx].psl != COBALT_HASHMAP_EMPTY_PSL) {
            break;
        }
        iter->slot_idx++;
    }
}

static int hashmap_map_iter_has_next(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    return (iter && iter->impl && iter->impl->slots && iter->slot_idx < iter->impl->capacity);
}

static cobalt_map_pair_t hashmap_map_iter_next(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    cobalt_map_pair_t   pair = {NULL, NULL};
    if (!hashmap_map_iter_has_next(ctx)) {
        return pair;
    }

    pair.key   = iter->impl->slots[iter->slot_idx].key;
    pair.value = iter->impl->slots[iter->slot_idx].value;

    iter->slot_idx++;
    hashmap_iter_advance(iter);
    return pair;
}

static void hashmap_map_iter_destroy(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    iter->alloc->free(iter->alloc, ctx);
}

static const cobalt_map_iterator_vtable_t hashmap_map_iter_vtable = {
    .has_next = hashmap_map_iter_has_next,
    .next     = hashmap_map_iter_next,
    .destroy  = hashmap_map_iter_destroy,
};

static cobalt_map_iterator_t *hashmap_map_iterator(cobalt_map_t *self);
static void                  *hashmap_map_get(cobalt_map_t *self, const void *key, size_t key_len);
static int    hashmap_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value);
static int    hashmap_map_remove(cobalt_map_t *self, const void *key, size_t key_len);
static size_t hashmap_map_size(cobalt_map_t *self);
static int    hashmap_map_is_empty(cobalt_map_t *self);
static void   hashmap_map_destroy(cobalt_map_t *self);
static int    hashmap_map_contains(cobalt_map_t *self, const void *key, size_t key_len);
static void   hashmap_map_clear(cobalt_map_t *self);

static const cobalt_map_t hashmap_map_vtable = {
    .get      = hashmap_map_get,
    .put      = hashmap_map_put,
    .remove   = hashmap_map_remove,
    .size     = hashmap_map_size,
    .is_empty = hashmap_map_is_empty,
    .destroy  = hashmap_map_destroy,
    .contains = hashmap_map_contains,
    .clear    = hashmap_map_clear,
    .iterator = hashmap_map_iterator,
};

static cobalt_map_iterator_t *hashmap_map_iterator(cobalt_map_t *self)
{
    cobalt_hashmap_t *map  = (cobalt_hashmap_t *)self;
    hashmap_impl_t   *impl = &map->impl;

    hashmap_map_iter_t *iter_state =
        (hashmap_map_iter_t *)impl->alloc->alloc(impl->alloc, sizeof(hashmap_map_iter_t));
    if (!iter_state) {
        return NULL;
    }
    iter_state->impl     = impl;
    iter_state->slot_idx = 0;
    iter_state->alloc    = impl->alloc;
    hashmap_iter_advance(iter_state);

    cobalt_map_iterator_t *iter =
        (cobalt_map_iterator_t *)impl->alloc->alloc(impl->alloc, sizeof(cobalt_map_iterator_t));
    if (!iter) {
        impl->alloc->free(impl->alloc, iter_state);
        return NULL;
    }
    iter->vtable = &hashmap_map_iter_vtable;
    iter->data   = iter_state;
    iter->alloc  = impl->alloc;
    return iter;
}

cobalt_map_iterator_t *cobalt_hashmap_iterator_create(cobalt_hashmap_t *map)
{
    if (!map) {
        return NULL;
    }
    return hashmap_map_iterator(&map->base);
}

static void *hashmap_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    return cobalt_hashmap_get_ext((const cobalt_hashmap_t *)self, key, key_len);
}

static int hashmap_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    return cobalt_hashmap_put_ext((cobalt_hashmap_t *)self, key, key_len, value);
}

static int hashmap_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    return cobalt_hashmap_remove_ext((cobalt_hashmap_t *)self, key, key_len);
}

static size_t hashmap_map_size(cobalt_map_t *self)
{
    return cobalt_hashmap_size((const cobalt_hashmap_t *)self);
}

static int hashmap_map_is_empty(cobalt_map_t *self)
{
    return cobalt_hashmap_size((const cobalt_hashmap_t *)self) == 0;
}

static void hashmap_map_destroy(cobalt_map_t *self)
{
    cobalt_hashmap_destroy((cobalt_hashmap_t *)self);
}

static int hashmap_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    return cobalt_hashmap_get_ext((const cobalt_hashmap_t *)self, key, key_len) != NULL;
}

static void hashmap_map_clear(cobalt_map_t *self)
{
    if (!self) {
        return;
    }
    cobalt_hashmap_t *map  = (cobalt_hashmap_t *)self;
    hashmap_impl_t   *impl = &map->impl;
    if (impl->slots) {
        for (size_t i = 0; i < impl->capacity; i++) {
            if (impl->slots[i].psl != COBALT_HASHMAP_EMPTY_PSL) {
                if (impl->slots[i].key_owned && impl->slots[i].key) {
                    impl->alloc->free(impl->alloc, impl->slots[i].key);
                }
            }
        }
        init_slots(impl->slots, impl->capacity);
        impl->size = 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API — lifecycle                                                     */
/* -------------------------------------------------------------------------- */

cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets)
{
    return cobalt_hashmap_create_with_allocator(initial_buckets, cobalt_allocator_get_system());
}

cobalt_hashmap_t *cobalt_hashmap_create_with_allocator(size_t              initial_buckets,
                                                       cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)alloc->alloc(alloc, sizeof(cobalt_hashmap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    map->base            = hashmap_map_vtable;
    hashmap_impl_t *impl = &map->impl;
    impl->alloc          = alloc;
    impl->size           = 0;
    impl->hash_func      = NULL;
    impl->equal_func     = NULL;

    if (initial_buckets > 0) {
        size_t pow2_count = hashmap_next_pow2(initial_buckets);
        impl->slots = (hashmap_slot_t *)alloc->alloc(alloc, sizeof(hashmap_slot_t) * pow2_count);
        if (!impl->slots) {
            alloc->free(alloc, map);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        init_slots(impl->slots, pow2_count);
        impl->capacity = pow2_count;
        impl->mask     = pow2_count - 1;
        impl->max_load = (pow2_count * 3) / 4;
    } else {
        impl->slots    = NULL;
        impl->capacity = 0;
        impl->mask     = 0;
        impl->max_load = 0;
    }

    return map;
}

cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t              initial_buckets,
                                            cobalt_hash_func_t  hash_func,
                                            cobalt_equal_func_t equal_func)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(initial_buckets);
    if (!map) {
        return NULL;
    }
    map->impl.hash_func  = hash_func;
    map->impl.equal_func = equal_func;
    return map;
}

int cobalt_hashmap_set_funcs(cobalt_hashmap_t   *map,
                             cobalt_hash_func_t  hash_func,
                             cobalt_equal_func_t equal_func)
{
    if (!map) {
        return -1;
    }
    map->impl.hash_func  = hash_func;
    map->impl.equal_func = equal_func;
    return 0;
}

void cobalt_hashmap_destroy(cobalt_hashmap_t *map)
{
    if (!map) {
        return;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->slots) {
        for (size_t i = 0; i < impl->capacity; i++) {
            if (impl->slots[i].psl != COBALT_HASHMAP_EMPTY_PSL) {
                if (impl->slots[i].key_owned && impl->slots[i].key) {
                    impl->alloc->free(impl->alloc, impl->slots[i].key);
                }
            }
        }
        impl->alloc->free(impl->alloc, impl->slots);
    }
    impl->alloc->free(impl->alloc, map);
}

/* -------------------------------------------------------------------------- */
/* Public API — operations                                                    */
/* -------------------------------------------------------------------------- */

int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    hashmap_impl_t *impl = &map->impl;
    size_t          len  = strlen(key);
    unsigned int    hash = slot_hash(impl, key, len);

    /* Update existing */
    size_t existing = find_slot(impl, key, len, hash);
    if (existing != (size_t)-1) {
        impl->slots[existing].value = value;
        return 0;
    }

    /* Resize if full */
    if (impl->capacity == 0 || impl->size >= impl->max_load) {
        size_t target_cap =
            impl->capacity == 0 ? COBALT_HASHMAP_DEFAULT_BUCKETS : (impl->capacity << 1);
        if (hashmap_ensure_capacity(impl, target_cap) != 0) {
            return -1;
        }
    }

    char *key_copy = cobalt_strdup_with_alloc(key, impl->alloc);
    if (!key_copy) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    hashmap_slot_t entry;
    entry.key       = key_copy;
    entry.value     = value;
    entry.hash      = hash;
    entry.psl       = 0;
    entry.key_len   = len;
    entry.key_owned = 1;

    robin_hood_insert_entry(impl->slots, impl->mask, entry);
    impl->size++;
    return 0;
}

int cobalt_hashmap_put_ext(cobalt_hashmap_t *map, const void *key, size_t key_len, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    hashmap_impl_t *impl = &map->impl;
    unsigned int    hash = slot_hash(impl, key, key_len);

    /* Update existing */
    size_t existing = find_slot(impl, key, key_len, hash);
    if (existing != (size_t)-1) {
        impl->slots[existing].value = value;
        return 0;
    }

    /* Resize if full */
    if (impl->capacity == 0 || impl->size >= impl->max_load) {
        size_t target_cap =
            impl->capacity == 0 ? COBALT_HASHMAP_DEFAULT_BUCKETS : (impl->capacity << 1);
        if (hashmap_ensure_capacity(impl, target_cap) != 0) {
            return -1;
        }
    }

    hashmap_slot_t entry;
    entry.key       = (void *)key;
    entry.value     = value;
    entry.hash      = hash;
    entry.psl       = 0;
    entry.key_len   = key_len;
    entry.key_owned = 0;

    robin_hood_insert_entry(impl->slots, impl->mask, entry);
    impl->size++;
    return 0;
}

void *cobalt_hashmap_get(const cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }

    const hashmap_impl_t *impl = &map->impl;
    size_t                len  = strlen(key);
    unsigned int          hash = slot_hash(impl, key, len);
    size_t                idx  = find_slot(impl, key, len, hash);
    if (idx == (size_t)-1) {
        cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
        return NULL;
    }
    return impl->slots[idx].value;
}

void *cobalt_hashmap_get_ext(const cobalt_hashmap_t *map, const void *key, size_t key_len)
{
    if (!map || !key) {
        return NULL;
    }

    const hashmap_impl_t *impl = &map->impl;
    unsigned int          hash = slot_hash(impl, key, key_len);
    size_t                idx  = find_slot(impl, key, key_len, hash);
    if (idx == (size_t)-1) {
        return NULL;
    }
    return impl->slots[idx].value;
}

int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    hashmap_impl_t *impl = &map->impl;
    size_t          len  = strlen(key);
    unsigned int    hash = slot_hash(impl, key, len);
    int             ret  = hashmap_remove_key(impl, key, len, hash);
    if (ret != 0) {
        cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
        return -1;
    }
    return 0;
}

int cobalt_hashmap_remove_ext(cobalt_hashmap_t *map, const void *key, size_t key_len)
{
    if (!map || !key) {
        return -1;
    }

    hashmap_impl_t *impl = &map->impl;
    unsigned int    hash = slot_hash(impl, key, key_len);
    return hashmap_remove_key(impl, key, key_len, hash);
}

size_t cobalt_hashmap_size(const cobalt_hashmap_t *map)
{
    return map ? map->impl.size : 0;
}

size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map)
{
    return map ? map->impl.capacity : 0;
}
