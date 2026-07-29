# Cobalt Tutorial: Getting Started with C11 OO Programming

Welcome to the Cobalt framework! This tutorial will guide you through installing Cobalt, understanding its architecture, and building your first application using its object-oriented capabilities.

## Prerequisites

- A C11-compliant compiler (GCC 7+, Clang 3.9+, or MSVC 2017+)
- CMake 3.14 or later
- Make or Ninja build tool
- Git (for cloning the repository)

## Step 1: Build and Install Cobalt

First, let's build the Cobalt library from source:

```bash
# Clone the repository
git clone https://github.com/cobalt-project/cobalt.git
cd cobalt

# Create a build directory and configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the library
make

# Optionally install system-wide
sudo make install  # or: DESTDIR=/tmp/stage make install
```

After building, you'll find the compiled library in `build/src/libcobalt.a` (static) or `build/src/libcobalt.so/.dll` (shared, depending on platform).

## Step 2: Understanding the Architecture

Cobalt follows an 8-layer modular architecture. Understanding these layers helps you choose the right tools for your task:

| Layer | Name | When You'll Use It |
|-------|------|-------------------|
| L8 | Platform Abstraction | Detecting OS/target, low-level atomics |
| L7a | Memory | Controlling how objects are allocated |
| L7b | Runtime | Error handling, logging/debug output |
| L6 | Core Object System | Creating classes, objects, interfaces |
| L5 | Container Interfaces | Working with sequences/maps generically |
| L4 | Concrete Collections | Using vector, list, hashmap, treemap |
| L3 | Algorithms | Sorting, searching, transforming data |
| L2 | Modules | JSON serialization, event loops |

Most applications start at L6 (objects) and L4 (containers), then explore L2 (JSON/eventloop) as needed.

## Step 3: Your First Cobalt Program - "Hello World" with Objects

Let's create a simple program that uses Cobalt's object system:

```c
// hello_world.c
#include <stdio.h>
#include <cobalt/cobalt.h>

/* Forward declare our custom class */
extern const cobalt_class_t GreetingClass;

int main(void) {
    /* Initialize logging to stdout at INFO level */
    FILE *log = stdout;
    cobalt_logger_init(log, LOG_LEVEL_INFO);
    
    cobalt_info("Starting Cobalt Hello World example
");
    
    /* Get platform ID */
    cobalt_platform_id_t platform = cobalt_platform_get_id();
    cobalt_info("Detected platform: %d
", platform);
    
    /* Create a simple custom class (Greeting) */
    /* In a real app, you'd define this properly with methods */
    /* For now we'll use a placeholder */
    cobalt_class_t *cls = cobalt_class_create("Greeting", NULL);
    
    /* Create an instance of our class */
    cobalt_object_t *obj = cobalt_object_new(cls, 0);
    
    /* Use reference counting to manage lifetime */
    cobalt_object_ref(obj);  /* Extra ref for demonstration */
    
    /* Do something with obj... */
    cobalt_info("Created object with class: %s
", 
                cobalt_object_get_class(obj)->name);
    
    /* Release refs */
    cobalt_object_unref(obj);  /* Drops first ref */
    cobalt_object_unref(obj);  /* Drops second, object freed */
    
    /* Clean up the class itself */
    cobalt_class_destroy(cls);
    
    cobalt_info("Done!
");
    return 0;
}
```

Compile and run:

```bash
gcc -o hello_world hello_world.c -I/include -Lbuild/src -lcobalt -ldl -pthread
./hello_world
```

## Step 4: Building a Real Data Structure - A User List

Now let's create something more practical: a list of user records stored in a vector, sorted by name.

```c
// user_list.c
#include <stdio.h>
#include <string.h>
#include <cobalt/cobalt.h>

typedef struct {
    char *name;
    int age;
} UserRecord;

/* Comparison function for sorting users by name */
int compare_users(const void *a, const void *b) {
    const UserRecord *ua = (const UserRecord *)a;
    const UserRecord *ub = (const UserRecord *)b;
    return strcmp(ua->name, ub->name);
}

int main(void) {
    /* Create a vector to store user records */
    cobalt_vector_t *users = cobalt_vector_create(10);  /* reserve 10 slots */
    
    /* Add some sample users */
    UserRecord alice = { .name = "Alice Smith", .age = 30 };
    UserRecord bob = { .name = "Bob Jones", .age = 25 };
    UserRecord charlie = { .name = "Charlie Brown", .age } = {35};
    
    /* Copy values into the vector (real app would allocate properly) */
    UserRecord *alice_copy = malloc(sizeof(UserRecord));
    memcpy(alice_copy, &alice, sizeof(UserRecord));
    cobalt_vector_push(users, alice_copy);
    
    UserRecord *bob_copy = malloc(sizeof(UserRecord));
    memcpy(bob_copy, &bob, sizeof(UserRecord));
    cobalt_vector_push(users, bob_copy);
    
    UserRecord *charlie_copy = malloc(sizeof(UserRecord));
    memcpy(charlie_copy, &charlie, sizeof(UserRecord));
    cobalt_vector_push(users, charlie_copy);
    
    /* Sort the vector by name */
    cobalt_qsort(users->items, users->size, sizeof(UserRecord), compare_users);
    
    /* Print sorted list */
    printf("Sorted Users:
");
    for (size_t i = 0; i < users->size; i++) {
        UserRecord *user = (UserRecord *)cobalt_vector_get(users, i);
        printf("  #%zu: %s (%d)
", i, user->name, user->age);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < users->size; i++) {
        UserRecord *user = (UserRecord *)cobalt_vector_get(users, i);
        free(user->name);
        free(user);
    }
    cobalt_vector_destroy(users);
    
    return 0;
}
```

## Step 5: Advanced Topic - Custom Memory Allocation with Arenas

For high-performance or real-time applications, you may want to avoid heap fragmentation by using arena allocators:

```c
// arena_example.c
#include <stdio.h>
#include <cobalt/cobalt.h>

int main(void) {
    /* Create a 64KB arena */
    cobalt_arena_t *arena = cobalt_arena_create(64 * 1024);
    if (!arena) {
        fprintf(stderr, "Failed to create arena
");
        return 1;
    }
    
    /* Use an arena-backed allocator */
    /* (Implementation detail: wrap arena in allocator interface) */
    // cobalt_allocator_t *arena_alloc = create_arena_allocator(arena);
    
    /* Now create objects that allocate from the arena */
    // Some constructors accept allocator parameters...
    
    /* When done with all objects, simply reset the arena */
    cobalt_arena_reset(arena);  /* Instant deallocation of ALL arena-allocated memory */
    
    /* Destroy arena itself */
    cobalt_arena_destroy(arena);
    
    return 0;
}
```

This pattern is ideal for game loops, request processing pipelines, or any scenario where you allocate many temporary objects within a single scope and can free them all at once.

## Step 6: Next Steps

Congratulations! You've completed the introductory tour. Here are recommended next steps:

1. **Read the full API reference** in `docs/API/cobalt.h.md` for complete function signatures
2. **Explore the SPEC documents** in `docs/SPEC/` for deep architectural details
3. **Look at the examples** in `examples/` for working code snippets
4. **Try building your own component** following the patterns shown
5. **Join the community** (mailing list, Discord, etc.) for help and discussion

Happy hacking with Cobalt!

---

*This tutorial accompanies Cobalt v2.0.0. See the CHANGELOG for updates between versions.*
