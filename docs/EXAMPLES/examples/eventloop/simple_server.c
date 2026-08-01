/**
 * @file simple_server.c
 * @brief Demonstrates basic event loop usage
 *
 * Shows:
 * - Creating an event loop
 * - Registering file descriptor handlers
 * - Running the loop
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>

/* Callback when a fd is ready */
void fd_callback(int fd, short events, void* user_data)
{
    (void)events;
    char* label = (char*)user_data;
    cobalt_info("File descriptor %d ready (%s)\n", fd, label);
}

/* Timer callback */
void timer_callback(uint64_t id, void* user_data)
{
    (void)user_data;
    cobalt_info("Timer % fired\n", id);
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Create an event loop */
    cobalt_eventloop_t* loop = cobalt_eventloop_create();
    if (!loop)
        {
            fprintf(stderr, "Failed to create event loop\n");
            return 1;
        }

    cobalt_info("Event loop created\n");

    /* In a real application, you would register actual FDs here */
    /* For this demo, we'll set up a timer as an example */
    uint64_t timer_id = cobalt_eventloop_add_timer(loop, 1000, 0, timer_callback, NULL);

    /* Run the event loop (this would block until stop() is called) */
    /* For demo purposes, we'll just do one iteration */
    int result = cobalt_eventloop_iteration(loop);
    cobalt_info("Iteration result: %d\n", result);

    /* Clean up */
    cobalt_eventloop_del_timer(loop, timer_id);
    cobalt_eventloop_destroy(loop);

    cobalt_info("Event loop demo complete!\n");
    return 0;
}