/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <pjsua-lib/pjsua.h>

#include "sip.h"
#include "sip_queue.h"
#include "threaded_timer.h"

#define PJSIP_AUTH_REALM "*"
#define PJSIP_AUTH_SCHEME "digest"

// --- PJSIP globals ---
action_queue_t g_sip_queue;
pthread_t sip_call_thread;
pthread_t sip_connection_thread;
pjsua_acc_id acc_id;
pjsua_acc_config acc_cfg;

// --- SIP user arguments configuration ---
char* sip_dest;
uint8_t call_timeout_seconds = 10;

// --- SIP call map ---

typedef struct {
    struct thread_timer_t timer;
} call_context_t;
pthread_mutex_t g_call_map_mutex = PTHREAD_MUTEX_INITIALIZER;
call_context_t g_call_map[PJSUA_MAX_CALLS];

// --- SIP connection flags ---

volatile int g_sip_registered = 0;
volatile int g_sip_registering = 0;
volatile int g_sip_fatally_shutdown = 0;

void stop_call(pjsua_call_id timed_call_id)
{
    action_t a = { .type = ACTION_HANGUP, .call_id = timed_call_id };
    printf("[SIP] Attempting to stop a RINGING call...\n");
    queue_push(&g_sip_queue, a);
}

static void start_registration()
{
    if (g_sip_registering)
        return;

    g_sip_registering = 1;
    if (!g_sip_registered && acc_id != PJSUA_INVALID_ID) {
        printf("[SIP] Attempting to register/re-register...\n");
        pjsua_acc_set_registration(acc_id, PJ_TRUE);
    }
}

void* pjsip_connection_thread(void* arg)
{
    pj_thread_desc desc;
    pj_thread_t *thread;
    pj_status_t status;

    // Register the thread with PJSIP
    status = pj_thread_register("connection_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        printf("Failed to register thread\n");
        return NULL;
    }

    unsigned retry_count = 0;
    while (1) {
        pjsua_handle_events(100);
        if (!g_sip_registered && !g_sip_registering) {
            if (retry_count < 3) {
                printf("[SIP] Trying to register (attempt %d)...\n", retry_count + 1);
                start_registration();
                retry_count++;
            } else {
                printf("[SIP] Registration failed after retries\n");
                g_sip_fatally_shutdown = 1;
                pause();
            }

            retry_count = 0;
        }
    }
}

void* pjsip_call_thread(void* arg)
{
    pj_thread_desc desc;
    pj_thread_t *thread;
    pj_status_t status;

    // Register the thread with PJSIP
    status = pj_thread_register("call_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        printf("Failed to register thread\n");
        return NULL;
    }

    while (1) {
        action_t a = queue_pop(&g_sip_queue);
        switch (a.type) {
            case ACTION_CALL:
                printf("[SIP] Making call\n");
                pj_str_t dest_uri = pj_str(sip_dest);
                pjsua_call_make_call(acc_id, &dest_uri, 0, NULL, NULL, NULL);
                break;
            case ACTION_HANGUP:
                printf("[SIP] Hanging up call %d\n", a.call_id);
                pj_status_t status =  pjsua_call_hangup(a.call_id, 0, NULL, NULL);
                if (status != PJ_SUCCESS) {
                    fprintf(stderr, "[SIP] Call hangup failed (not registered or transport issue)\n");
                }
                break;
        }
    }
    return NULL;
}

void on_reg_state(pjsua_acc_id acc_id)
{
    pjsua_acc_info info;
    pjsua_acc_get_info(acc_id, &info);

    if (info.status == 200) {
        printf("[SIP] Registered successfully\n");
        g_sip_registered = 1;
        g_sip_registering = 0;
    } else {
        printf("[SIP] Registration failed: %d (%.*s)\n",
               info.status,
               (int)info.status_text.slen,
               info.status_text.ptr);

        g_sip_registered = 0;
        g_sip_registering = 0;
    }
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        printf("[SIP] Call %d disconnected\n", call_id);
        pthread_mutex_lock(&g_call_map_mutex);
        struct thread_timer_t *t = &g_call_map[call_id].timer;
        atomic_store(&t->canceled, 1);
        pthread_mutex_unlock(&g_call_map_mutex);
    } else if (ci.state == PJSIP_INV_STATE_EARLY) {
        printf("[SIP] Call ringing\n");

        // We now are ringing, queue a timer to stop it after the specified timeout!
        pthread_mutex_lock(&g_call_map_mutex);
        struct thread_timer_t *t = &g_call_map[call_id].timer;
        atomic_store(&t->canceled, 0);
        if (timer_start(t) != 0) {
            action_t a = { .type = ACTION_HANGUP, .call_id = call_id };
            printf("[SIP] Failed to start a timer thread, hangup immediately...\n");
            queue_push(&g_sip_queue, a);
            return;
        }
        pthread_mutex_unlock(&g_call_map_mutex);

    } else if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        printf("[SIP] Call confirmed\n");
    } else if (ci.state == PJSIP_INV_STATE_CONNECTING) {
        printf("[SIP] Call connecting\n");
    }
}

int pjsip_init_app(struct sip_config *run_cfg)
{
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pj_status_t status;
    pjsua_transport_id tcp_id;

    pjsua_create();

    pjsua_config_default(&cfg);
    cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_reg_state = &on_reg_state;
    cfg.cb.on_incoming_call = NULL;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 0;
    log_cfg.level = 0;

    pjsua_init(&cfg, &log_cfg, NULL);

    pjsua_transport_config tcfg;
    pjsua_transport_config_default(&tcfg);
    tcfg.port = run_cfg->port;

    if (run_cfg->use_tcp_transport) {
        status = pjsua_transport_create(PJSIP_TRANSPORT_TCP, &tcfg, &tcp_id);
    } else {
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, &tcp_id);
    }

    if (status != PJ_SUCCESS) {
        printf("Error creating transport for SIP\n");
        return RC_FAILURE;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS)
        return RC_FAILURE;

    pjsua_set_null_snd_dev();

    /* Configure SIP account */
    pjsua_acc_config_default(&acc_cfg);

    acc_cfg.id = pj_str(run_cfg->id);
    acc_cfg.reg_uri = pj_str(run_cfg->reg_uri);
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].realm = pj_str(PJSIP_AUTH_REALM);
    acc_cfg.cred_info[0].scheme = pj_str(PJSIP_AUTH_SCHEME);
    acc_cfg.cred_info[0].username = pj_str(run_cfg->user);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data = pj_str(run_cfg->pass);

    /* Force TCP transport */
    acc_cfg.transport_id = tcp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &acc_id);

    if (status != PJ_SUCCESS) {
        printf("SIP Account add failed\n");
        return RC_FAILURE;
    }

    printf("SIP TCP client running...\n");
    return RC_OK;
}

return_code_t initialize_sip_client(struct sip_config *cfg)
{
    call_timeout_seconds = cfg->call_timeout_seconds;
    sip_dest = cfg->callee_uri;

    for (int i = 0; i < PJSUA_MAX_CALLS; i++) {
        struct thread_timer_t* t = &g_call_map[i].timer;
        t->seconds = call_timeout_seconds;
        t->callback = stop_call;
        t->call_id = i;
        atomic_init(&t->canceled, 0);
    }

    /* Initialize PJSIP with our runtime configuration */
    int rc = pjsip_init_app(cfg);
    if (rc != RC_OK)
        return RC_FAILURE;

    /* Start PJSIP threads - one for monitoring the connection
     * and another one for tracking call requests from a queue.
     */
    pthread_create(&sip_call_thread, NULL, pjsip_call_thread, NULL);
    pthread_create(&sip_connection_thread, NULL, pjsip_connection_thread, NULL);
    return RC_OK;
}

void stop_sip_client()
{
    pthread_join(sip_call_thread, NULL);
    pthread_join(sip_connection_thread, NULL);
    pjsua_destroy();
}
