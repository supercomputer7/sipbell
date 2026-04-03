/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include "threaded_timer.h"

void *timer_thread_func(void *arg) {
    struct timespec req;

    struct thread_timer_t *t = (struct thread_timer_t *)arg;

    while (1) {
        req.tv_sec = 0;               // seconds
        req.tv_nsec = 100 * 1000000;  // 100 milliseconds = 100,000,000 nanoseconds

        if (nanosleep(&req, NULL) == -1) {
            perror("nanosleep");
        }

        pthread_mutex_lock(&t->mutex);
        if (t->cancelled) {
            pthread_mutex_unlock(&t->mutex);
            continue;
        }

        if (t->milliseconds_left == 0) {
            t->cancelled = true;
            t->callback(t->call_id);
        } else {
            t->milliseconds_left -= 100;
        }

        pthread_mutex_unlock(&t->mutex);
    }

    return NULL;
}

int threaded_timer_create(struct thread_timer_t *t) {
    int rc = pthread_create(&t->thread, NULL, timer_thread_func, t);
    if (rc < 0)
        return 1;
    rc = pthread_detach(t->thread);
    if (rc < 0)
        return 1;
    return 0;
}
