# SPEC: Core Object System (Layer 6)

**Module:** `include/cobalt/core/`, `src/core/`  
**Files:** object.h, object.c, class.h, class.c, interface.h, interface.c

## 1. Overview

The Core Object System provides the foundation for Cobalt's object-oriented capabilities in C11. It implements single inheritance with multi-interface support, read-only vtables, RTTI type checking, and atomic reference counting.

## 2. Data Structures

### 2.1 CobaltObject (Base Structure)

All public objects begin with this header:

```c
typedef struct cobalt_object {
    uint64_t ref_count;       // Atomic reference count
    cobalt_class_t *class;    // Pointer to class metadata
} cobalt_object_t;
```

The object layout ensures ABI compatibility - any pointer to a derived object can be safely cast to `cobalt_object_t*`.

### 2.2 CobaltClass (Type Metadata)

```c
typedef struct cobalt_method {
    const char *name;
    void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count);
} cobalt_method_t;

typedef struct cobalt_property {
    const char *name;
    void *(*get)(cobalt_object_t *self);
    void (*set)(cobalt_object_t *self, void *value);
} cobalt_property_t;

typedef struct cobalt_class {
    const char *name;               // Class name (RTTI)
    size_t method_count;
    cobalt_method_t **methods;      // Vtable (read-only after init)
    size_t property_count;
    cobalt_property_t **properties;
    cobalt_class_t *base_class;     // Single inheritance parent
    int abstract;                   // Abstract class flag
} cobalt_class_t;
```

Vtables are initialized at build time or during static initialization and then made read-only by placing them in `.rodata`.

## 3. API Reference

### 3.1 Object Lifecycle

| Function | Description |
|----------|-------------|
| `cobalt_object_new(cls, extra_size)` | Allocate new object with optional extra space |
| `cobalt_object_ref(obj)` | Increment reference count |
| `cobalt_object_unref(obj)` | Decrement; free when count reaches zero |
| `cobalt_object_get_class(obj)` | Return class metadata for RTTI |

### 3.2 Class Management

| Function | Description |
|----------|-------------|
| `cobalt_class_create(name, base)` | Create a new class (with optional base class) |
| `cobalt_class_add_method(cls, name, invoke)` | Register a method on the class |
| `cobalt_class_destroy(cls)` | Free class resources |

### 3.3 Interface Querying

```c
// Check if an object supports an interface
int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface);

// Get interface vtable (similar to COM QueryInterface)
void *cobalt_object_query_interface(cobalt_object_t *obj, const char *iface_name);
```

## 4. Usage Example

```c
// Create a class hierarchy
cobalt_class_t *base_cls = cobalt_class_create("Base", NULL);
cobalt_class_t *derived_cls = cobalt_class_create("Derived", base_cls);

// Create an instance
cobalt_object_t *obj = cobalt_object_new(derived_cls, sizeof(MyData));

// Use interfaces (multi-injection)
cobalt_interface_t *seq_iface = create_sequence_iface();
cobalt_object_inject_interface(obj, seq_iface);

// Reference counted lifecycle
cobalt_object_ref(obj);
// ... use obj ...
cobalt_object_unref(obj);
```

## 5. Thread Safety

- Reference count operations use `stdatomic` guarantees (LOCK-FREE on target platform)
- Vtables are immutable after construction → safe for concurrent reads
- Class creation is not thread-safe; must be serialized during initialization phase

## 6. Memory Allocation

Objects are allocated using an allocator injected at construction time. The default system allocator (`malloc`/`free`) may be overridden with custom allocators (Arena, Pool, Slab) for specialized memory management strategies.
