#API Reference — Core Object System(Layer 6)

**Module : ** `include / cobalt / core /` **Dependencies : ** `platform.h`, `atomic.h`

                                                                                ##Overview

                                                                                    The core object
                                                                                        system provides
                                                                                            single -
                                                                                inheritance class
                                                                                support,
    atomic reference counting,
    and RTTI(Runtime Type Information)
            .Every public object in Cobalt embeds a `cobalt_object_t` header as its first member.

        ##Object Lifecycle

```c
#include "cobalt/core/object.h"

        cobalt_class_t *cls = cobalt_class_create("MyClass", NULL);
cobalt_object_t *obj        = cobalt_object_new(cls, sizeof(MyData));

cobalt_object_ref(obj);   // ref_count: 1 → 2
cobalt_object_unref(obj); // ref_count: 2 → 1
cobalt_object_unref(obj); // ref_count: 1 → 0, freed
```

    ## #Functions

    | Function | Description | | -- -- -- -- --| -- -- -- -- -- -- -|
    | `cobalt_object_new(cls, extra_size)` | Allocate object with ref_count =
    1 | | `cobalt_object_ref(obj)` | Increment reference count(thread - safe) |
    | `cobalt_object_unref(obj)` | Decrement; free when count reaches 0 |
| `cobalt_object_get_class(obj)` | Get class metadata for RTTI |
| `cobalt_object_get_ref_count(obj)` | Get current ref count |

### Null Safety

All functions handle NULL gracefully — `cobalt_object_ref(NULL)` and `cobalt_object_unref(NULL)` are no-ops.

## Class System

```c
typedef struct cobalt_class cobalt_class_t;

cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class);
int             cobalt_class_add_method(cobalt_class_t *cls,
                                        const char     *name,
                                        void *(*invoke)(cobalt_object_t *, void **, size_t));
int             cobalt_class_add_property(cobalt_class_t *cls,
                                          const char     *name,
                                          void *(*get)(cobalt_object_t *),
                                          void (*set)(cobalt_object_t *, void *));
int             cobalt_class_is_abstract(cobalt_class_t *cls);
void            cobalt_class_destroy(cobalt_class_t *cls);
```

    - Single inheritance                     via `base_class` parameter -
    Methods and properties are registered at runtime -
    Vtables are placed in `.rodata` — immutable after construction -
    Class creation is * * not thread - safe * *;
serialize during initialization

    ##Interface System

```c typedef struct cobalt_interface cobalt_interface_t;

cobalt_interface_t *cobalt_interface_new(cobalt_interface_vtable_t *vtable);
void                cobalt_interface_destroy(cobalt_interface_t *iface);
int                 cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface);
```

    The `cobalt_interface_t` structure supports multi -
    interface implementation.Currently only the destructor is required in the vtable; method dispatch uses runtime `implements()` checks.

## Thread Safety

- `cobalt_object_ref/unref` use C11 atomics (LOCK-FREE on supported platforms)
- Vtable reads are safe for concurrent access (read-only after init)
- Class/interface creation must be serialized
