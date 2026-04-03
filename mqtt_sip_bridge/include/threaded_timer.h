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
    atomic_int canceled;
    pjsua_call_id call_id;
};

int timer_start(struct thread_timer_t *t);

#endif
