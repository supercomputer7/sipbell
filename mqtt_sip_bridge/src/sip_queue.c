/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include "sip_queue.h"

void queue_init(action_queue_t *q)
{
    q->head = q->tail = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

int queue_push(action_queue_t *q, action_t action)
{
    pthread_mutex_lock(&q->mutex);

    int next = (q->tail + 1) % QUEUE_SIZE;
    if (next == q->head) {
        // queue full
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    q->items[q->tail] = action;
    q->tail = next;

    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

action_t queue_pop(action_queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    while (q->head == q->tail) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }

    action_t action = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;

    pthread_mutex_unlock(&q->mutex);
    return action;
}
