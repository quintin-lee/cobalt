/**
 * @file object_oriented.c
 * @brief Demonstrate Cobalt object system
 */

#include "cobalt/cobalt.h"
#include <stdio.h>
#include <inttypes.h>

/* Forward declaration */
static cobalt_class_t *ShapeClass  = NULL;
static cobalt_class_t *CircleClass = NULL;

/* Base class method */
static void *shape_area(cobalt_object_t *self, void **args, size_t arg_count)
{
    (void)self;
    (void)args;
    (void)arg_count;
    printf("  Shape area method called\n");
    return NULL;
}

/* Circle specific method */
static void *circle_area(cobalt_object_t *self, void **args, size_t arg_count)
{
    (void)self;
    (void)arg_count;
    double radius = args[0] ? *(double *)args[0] : 1.0;
    double area   = 3.14159 * radius * radius;
    printf("  Circle area: %.2f\n", area);
    return NULL;
}

int main(void)
{
    printf("=== Object Oriented Example ===\n\n");

    /* Initialize classes */
    ShapeClass  = cobalt_class_create("Shape", NULL);
    CircleClass = cobalt_class_create("Circle", ShapeClass);

    /* Add methods */
    cobalt_class_add_method(ShapeClass, "area", shape_area);
    cobalt_class_add_method(CircleClass, "area", circle_area);

    /* Create object */
    cobalt_object_t *circle = cobalt_object_new(CircleClass, 0);
    printf("Created circle object\n");

    /* Reference counting */
    cobalt_object_ref(circle);
    printf("Ref count: %" PRIu64 "\n", circle->ref_count);

    cobalt_object_unref(circle);
    printf("After unref, object freed\n");

    /* Cleanup */
    cobalt_class_destroy(CircleClass);
    cobalt_class_destroy(ShapeClass);

    printf("\n=== Example completed ===\n");
    return 0;
}
