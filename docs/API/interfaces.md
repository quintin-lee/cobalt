#API Reference — Polymorphic Interfaces(Layer 5)

**Module : ** `include / cobalt / interface /` **Dependencies : ** `sequence.h`, `map.h`, `iterator
                                                                                                  .h`

                                                                                              ##Overview

                                                                                                  Cobalt defines
                                                                                                      two abstract interfaces
                                                                                                          that concrete containers
                                                                                                              embed to enable
                                                                                                                  polymorphic
                                                                                                                      usage
    : `cobalt_sequence_t` (ordered collections) and `cobalt_map_t` (key - value mappings)
                                                                                                          .An
                                                                                                      iterator
                                                                                                      system
                                                                                                      traverses
                                                                                                      both.

                                                                                                      ##Sequence
                                                                                                      Interface

                                                                                                      Containers
                                                                                                      implementing `cobalt_sequence_t` support
                                                                                                      indexed
                                                                                                      traversal
                                                                                                          and polymorphic
                                                                                                      add
                                                                                                      /
                                                                                                      remove
                                                                                                      operations
                                                                                                          .

                                                                                                      ## #Implementors

                                                                                                  |
                                                                                                  Container
                                                                                                  |
                                                                                                  Header
                                                                                                  |
                                                                                                  Sequence
                                                                                                  Support
                                                                                                  |
                                                                                                  |
                                                                                                  -- -- -- -- -- -|
                                                                                                  -- -- -- --|
                                                                                                  -- -- -- -- -- -- -- -- -|
                                                                                                  |
                                                                                                  Vector
                                                                                                  | `container
                                                                                                        /
                                                                                                        vector
                                                                                                            .h` | ✅ First
                                                                                                                      -
                                                                                                                      member
                                                                                                                      embed
                                                                                                  |
                                                                                                  |
                                                                                                  List
                                                                                                  | `container
                                                                                                        /
                                                                                                        list.h` | ✅ First
                                                                                                                      -
                                                                                                                      member
                                                                                                                      embed
                                                                                                  |
                                                                                                  |
                                                                                                  Deque
                                                                                                  | `container
                                                                                                        /
                                                                                                        deque
                                                                                                            .h` | ✅ First
                                                                                                                      -
                                                                                                                      member
                                                                                                                      embed
                                                                                                  |
                                                                                                  |
                                                                                                  Stack
                                                                                                  | `container
                                                                                                        /
                                                                                                        stack
                                                                                                            .h` | ❌ LIFO
                                                                                                                      -
                                                                                                                      only
                                                                                                                      semantics
                                                                                                  |
                                                                                                  |
                                                                                                  Queue
                                                                                                  | `container
                                                                                                        /
                                                                                                        queue
                                                                                                            .h` | ❌ FIFO
                                                                                                                      -
                                                                                                                      only
                                                                                                                      semantics
                                                                                                  |

                                                                                                  ## #Vtable
                                                                                                  Methods

```c struct cobalt_sequence {
    size_t (*size)(cobalt_sequence_t *self);
    int (*is_empty)(cobalt_sequence_t *self);
    void (*add)(cobalt_sequence_t *self, void *item);
    void (*remove)(cobalt_sequence_t *self, void *item);
    cobalt_iterator_t *(*iterator)(cobalt_sequence_t *self);
    void *(*get_at_index)(cobalt_sequence_t *self, size_t index);
};
```

    ## #Convenience Functions

```c cobalt_sequence_size(seq);   // Returns element count
cobalt_sequence_is_empty(seq);     // Returns 1 if empty
cobalt_sequence_add(seq, item);    // Adds element; returns 0 on success
cobalt_sequence_remove(seq, item); // Removes by pointer equality; returns 0 if found, -1 if not
```

    ## #Polymorphic Usage

```c cobalt_sequence_t *seq = (cobalt_sequence_t *)cobalt_vector_create(4);
cobalt_sequence_add(seq, &value);
cobalt_sequence_t *seq2 = (cobalt_sequence_t *)cobalt_deque_create();
cobalt_sequence_add(seq2, &value);
```

        ##Map Interface

            Containers implementing `cobalt_map_t` support key -
        value                                              operations.

        ## #Implementors

    | Container | Header | Map Support | | -- -- -- -- -- -| -- -- -- --| -- -- -- -- -- -- -| |
    HashMap | `container / hashmap.h` | ✅ First - member embed | |
    TreeMap | `container / treemap.h` | ✅ First - member embed | |
    Set | `container / set.h` | ✅ First - member         embed(value = key) |

    ## #Vtable Methods

```c struct cobalt_map {
    void *(*get)(cobalt_map_t *self, const void *key, size_t key_len);
    int (*put)(cobalt_map_t *self, const void *key, size_t key_len, void *value);
    int (*remove)(cobalt_map_t *self, const void *key, size_t key_len);
    int (*contains)(cobalt_map_t *self, const void *key, size_t key_len);
    void (*clear)(cobalt_map_t *self);
    size_t (*size)(cobalt_map_t *self);
    int (*is_empty)(cobalt_map_t *self);
    cobalt_map_iterator_t *(*iterator)(cobalt_map_t *self);
    void (*destroy)(cobalt_map_t *self);
};
```

    ## #Convenience Functions

```c cobalt_map_get(map, key, key_len);
cobalt_map_put(map, key, key_len, value);
cobalt_map_remove(map, key, key_len);
cobalt_map_contains(map, key, key_len);
cobalt_map_clear(map);
cobalt_map_size(map);
cobalt_map_is_empty(map);
```

    ## #Polymorphic Usage

```c cobalt_map_t *map = (cobalt_map_t *)cobalt_hashmap_create(8);
cobalt_map_put(map, "key", 4, value);
cobalt_map_t *map2 = (cobalt_map_t *)cobalt_treemap_create();
cobalt_map_put(map2, "key", 4, value);
```

    ##Iterator

```c
#include "cobalt/interface/iterator.h"

    cobalt_iterator_t *cobalt_iterator_new(cobalt_sequence_t *seq);
int                    cobalt_iterator_has_next(cobalt_iterator_t *iter);
void                  *cobalt_iterator_next(cobalt_iterator_t *iter);
void                   cobalt_iterator_destroy(cobalt_iterator_t *iter);
```

    The generic iterator uses `get_at_index` from the sequence interface.For map and set iterators,
    use the concrete factory functions :

```c cobalt_map_iterator_t *iter = cobalt_hashmap_iterator_create(map);
while (cobalt_map_iterator_has_next(iter)) {
    cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
    // pair.key and pair.value
}
cobalt_map_iterator_destroy(iter);
```

    ##Macro Helpers

```c
    // For-each over any sequence
    cobalt_foreach(item, seq)
{
    // item is each element pointer
}

// Reverse iteration over vectors
cobalt_foreach_rev(item, vec)
{
    // item is each element pointer
}
```
