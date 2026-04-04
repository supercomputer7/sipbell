/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#ifndef __SIP_QUEUE_H__
#define __SIP_QUEUE_H__

#include <pthread.h>
#include <stdlib.h>
#include <pjsua-lib/pjsua.h>

typedef enum {
    ACTION_CALL,
    ACTION_HANGUP,
    ACTION_HANGUP_ALL
} action_type_t;

typedef struct {
    action_type_t type;
    pjsua_call_id call_id;
} action_t;


#define QUEUE_SIZE 16

typedef struct {
    action_t items[QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} action_queue_t;

void queue_init(action_queue_t *q);
int queue_push(action_queue_t *q, action_t action);
action_t queue_pop(action_queue_t *q);

#endif
