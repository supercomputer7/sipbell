/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include "threaded_timer.h"

void *timer_thread_func(void *arg) {
    struct timespec req;

    struct thread_timer_t *t = (struct thread_timer_t *)arg;
    for (int i = 0; i < t->seconds * 10; i++) {
        if (atomic_load(&t->canceled))
            goto timer_thread_exit;

        req.tv_sec = 0;               // seconds
        req.tv_nsec = 100 * 1000000;  // 100 milliseconds = 100,000,000 nanoseconds

        if (nanosleep(&req, NULL) == -1) {
            perror("nanosleep");
            goto timer_thread_exit;
        }
    }
    if (!atomic_load(&t->canceled))
        t->callback(t->call_id);

timer_thread_exit:
    return NULL;
}

int timer_start(struct thread_timer_t *t) {
    int rc = pthread_create(&t->thread, NULL, timer_thread_func, t);
    if (rc < 0)
        return 1;
    rc = pthread_detach(t->thread);
    if (rc < 0)
        return 1;
    return 0;
}
