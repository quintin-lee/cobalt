# SPEC: Container Interfaces (Layer 5)

**Module:** `include/cobalt/interface/`  
**Files:** sequence.h, map.h, iterator.h

## 1. Overview

Container interfaces define pure virtual contracts that concrete collections implement. They enable algorithm-layer independence: algorithms operate on interfaces, not concrete types.

## 2. Sequence Interface

Represents ordered collections (indexed sequences):

```c
typedef struct cobalt_sequence cobalt_sequence_t;

typedef struct {
    size_t (*size)(cobalt_sequence_t *self);
    int (*is_empty)(cobalt_sequence_t *self);
    void (*add)(cobalt_sequence_t *self, void *item);
    void (*remove)(cobalt_sequence_t *self, void *item);
    cobalt_iterator_t *(*iterator)(cobalt_sequence_t *self);
} cobalt_sequence_ops;

struct cobalt_sequence {
    const cobalt_sequence_ops *ops;  // Vtable pointer
};
```

Implementations embed `cobalt_sequence_t` as their first member, ensuring vtable dispatch works through polymorphic access.

## 3. Map Interface

Represents key-value mappings:

```c
typedef struct cobalt_map cobalt_map_t;

typedef struct {
    void *(*get)(cobalt_map_t *self, const void *key);
    int (*put)(cobalt_map_t *self, const void *key, void *value);
    int (*remove)(cobalt_map_t *self, const void *key);
    size_t (*size)(cobalt_map_t *self);
    int (*is_empty)(cobalt_map_t *self);
} cobalt_map_ops;

struct cobalt_map {
    const cobalt_map_ops *ops;
};
```

Both HashMap and TreeMap implement this interface.

## 4. Iterator Interface

Provides traversal over collections:

```c
typedef struct cobalt_iterator cobalt_iterator_t;

int cobalt_iterator_has_next(cobalt_iterator_t *iter);
void *cobalt_iterator_next(cobalt_iterator_t *iter);
void cobalt_iterator_destroy(cobalt_iterator_t *iter);
```

Iterators are obtained from container-specific iterator factory methods (e.g., `sequence->iterator(seq)`).

## 5. Design Notes

- All interfaces use explicit vtable pointers rather than hidden this-passing — makes ABI stable across compilation units
- No heap allocation within interface methods themselves (all operations use self-contained memory)
- Interfaces are designed to be embeddable as first members of concrete structures
