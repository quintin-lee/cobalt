/**
 * @file application_skeleton.c
 * @brief Production-style application skeleton using Cobalt
 *
 * Demonstrates typical application structure:
 *   1. Early initialization (platform, logger, classes)
 *   2. Service/component construction
 *   3. Main event or polling loop
 *   4. Graceful shutdown with resource cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <cobalt/cobalt.h>

/* Application state */
typedef struct {
    cobalt_eventloop_t *event_loop;
    cobalt_allocator_t *allocator;
    cobalt_arena_t *arena;
    volatile int running;
} AppContext;

/* Global context for signal handlers */
static AppContext *app_ctx = NULL;

/* Signal handler for graceful shutdown */
void sig_handler(int sig) {
    (void)sig;
    if (app_ctx && app_ctx->running) {
        app_ctx->running = 0;
        cobalt_info("Received signal %d, shutting down...\n", sig);
        if (app_ctx->event_loop) {
            cobalt_eventloop_stop(app_ctx->event_loop);
        }
    }
}

/* Initialize the application */
int app_init(AppContext *ctx) {
    ctx->running = 1;
    ctx->event_loop = NULL;
    ctx->allocator = NULL;
    ctx->arena = NULL;

    /* Step 1: Initialize platform and logger */
    cobalt_platform_id_t platform = cobalt_platform_get_id();
    cobalt_info("Starting Cobalt application on platform %d\n", platform);

    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Step 2: Create arena allocator for transient allocations */
    ctx->arena = cobalt_arena_create(64 * 1024);
    if (!ctx->arena) {
        fprintf(stderr, "Failed to create arena\n");
        return -1;
    }

    /* Step 3: Create event loop */
    ctx->event_loop = cobalt_eventloop_create();
    if (!ctx->event_loop) {
        fprintf(stderr, "Failed to create event loop\n");
        cobalt_arena_destroy(ctx->arena);
        return -1;
    }

    cobalt_info("Application initialized successfully\n");
    return 0;
}

/* Cleanup application resources */
void app_cleanup(AppContext *ctx) {
    if (ctx->event_loop) {
        cobalt_eventloop_destroy(ctx->event_loop);
        ctx->event_loop = NULL;
    }
    if (ctx->arena) {
        cobalt_arena_reset(ctx->arena);
        cobalt_arena_destroy(ctx->arena);
        ctx->arena = NULL;
    }
    cobalt_info("Application cleaned up\n");
}

/* Main application loop */
int app_main(AppContext *ctx) {
    while (ctx->running) {
        /* Process one iteration of the event loop */
        int ret = cobalt_eventloop_iteration(ctx->event_loop);
        if (ret < 0) {
            cobalt_error("Event loop iteration failed\n");
            break;
        }

        /* Do other work here if not blocking on the event loop */

        /* Optional: small sleep to prevent busy-waiting if no events */
        /* usleep(1000); */
    }

    return 0;
}

int main(void) {
    AppContext ctx = {0};

    /* Set up signal handling for graceful shutdown */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    app_ctx = &ctx;

    if (app_init(&ctx) != 0) {
        fprintf(stderr, "Application initialization failed\n");
        return 1;
    }

    int result = app_main(&ctx);

    app_cleanup(&ctx);

    cobalt_info("Application exited with code %d\n", result);
    return result;
}