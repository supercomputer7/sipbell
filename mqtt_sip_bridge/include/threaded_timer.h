/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#ifndef __THREADED_TIMER_H__
#define __THREADED_TIMER_H__

#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include <pjsua-lib/pjsua.h>

struct thread_timer_t {
    int seconds;
    void (*callback)(pjsua_call_id);
    pthread_t thread;
    pjsua_call_id call_id;

    /* These 2 fields should be protected by the mutex */
    bool cancelled;
    unsigned milliseconds_left;

    pthread_mutex_t mutex;
};

int threaded_timer_create(struct thread_timer_t *t);

#endif
