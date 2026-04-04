/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <pjsua-lib/pjsua.h>
#include <signal.h>

#include "sip.h"
#include "sip_queue.h"
#include "threaded_timer.h"

#define PJSIP_AUTH_REALM "*"
#define PJSIP_AUTH_SCHEME "digest"

extern volatile sig_atomic_t g_stop;

// --- PJSIP globals ---
action_queue_t g_sip_queue;
pthread_t sip_call_thread;
pthread_t sip_registration_thread;
pjsua_acc_id acc_id;
pjsua_acc_config acc_cfg;

// --- SIP user arguments configuration ---
char* sip_dest;
uint8_t call_timeout_seconds = 10;

// --- SIP call map ---

typedef struct {
    struct thread_timer_t timer;
} call_context_t;
call_context_t g_call_map[PJSUA_MAX_CALLS];

// --- SIP registration flags ---

volatile int g_sip_registered = 0;
volatile int g_sip_registering = 0;
volatile int g_sip_fatally_shutdown = 0;

volatile sig_atomic_t g_sip_stop_registration_thread = 0;
volatile sig_atomic_t g_sip_stop_all_threads = 0;

void stop_call(pjsua_call_id timed_call_id)
{
    action_t a = { .type = ACTION_HANGUP, .call_id = timed_call_id };
    fprintf(stderr, "[SIP] Attempting to stop a RINGING call...\n");
    queue_push(&g_sip_queue, a);
}

static void start_registration()
{
    if (g_sip_registering)
        return;

    g_sip_registering = 1;
    if (!g_sip_registered && acc_id != PJSUA_INVALID_ID) {
        fprintf(stderr, "[SIP] Attempting to register/re-register...\n");
        pjsua_acc_set_registration(acc_id, PJ_TRUE);
    }
}

void* pjsip_registration_thread(void* arg)
{
    pj_thread_desc desc;
    pj_thread_t *thread;
    pj_status_t status;

    // Register the thread with PJSIP
    status = pj_thread_register("registration_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Failed to register thread\n");
        return NULL;
    }

    unsigned retry_count = 0;
    while (!g_sip_stop_registration_thread) {
        pjsua_handle_events(100);
        if (!g_sip_registered && !g_sip_registering) {
            if (retry_count < 3) {
                fprintf(stderr, "[SIP] Trying to register (attempt %d)...\n", retry_count + 1);
                start_registration();
                retry_count++;
            } else {
                fprintf(stderr, "[SIP] Registration failed after retries\n");
                g_sip_fatally_shutdown = 1;
                pause();
            }

            retry_count = 0;
        }
    }

    fprintf(stderr, "[SIP] Exiting registration thread\n");

    return NULL;
}

void* pjsip_call_thread(void* arg)
{
    pj_thread_desc desc;
    pj_thread_t *thread;
    pj_status_t status;

    // Register the thread with PJSIP
    status = pj_thread_register("call_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Failed to register thread\n");
        return NULL;
    }

    while (1) {
        action_t a = queue_pop(&g_sip_queue);
        switch (a.type) {
            case ACTION_CALL:
                fprintf(stderr, "[SIP] Making call\n");
                pj_str_t dest_uri = pj_str(sip_dest);

                pjsua_call_setting call_settings;
                pjsua_call_setting_default(&call_settings);
                call_settings.aud_cnt = 1;
                call_settings.vid_cnt = 0;
                call_settings.flag |= PJSUA_CALL_INCLUDE_DISABLED_MEDIA;
                status = pjsua_call_make_call(acc_id, &dest_uri, &call_settings, NULL, NULL, NULL);
                if (status != PJ_SUCCESS) {
                    fprintf(stderr, "[SIP] Making call failed (not registered, transport issue or too many calls at once)\n");
                }
                break;
            case ACTION_HANGUP:
                fprintf(stderr, "[SIP] Hanging up call %d\n", a.call_id);
                status = pjsua_call_hangup(a.call_id, 0, NULL, NULL);
                if (status != PJ_SUCCESS) {
                    fprintf(stderr, "[SIP] Call hangup failed (not registered or transport issue)\n");
                }
                break;
            case ACTION_HANGUP_ALL:
                fprintf(stderr, "[SIP] Hanging up all calls\n");
                pjsua_call_hangup_all();
                fprintf(stderr, "[SIP] Hanging up all calls, done...\n");
                // FIXME: This kinda a hack, but we hang up all calls and check if g_stop is set
                // so we break out of the thread loop completely.
                if (g_stop) {
                    fprintf(stderr, "[SIP] Exiting call thread\n");
                    return NULL;
                }
                break;
        }
    }
}

void on_reg_state(pjsua_acc_id acc_id)
{
    /* At this point, we don't have a registration anymore, 
       so ignore every registation state change*/
    if (g_sip_stop_all_threads)
        return;

    pjsua_acc_info info;
    pjsua_acc_get_info(acc_id, &info);

    if (g_stop) {
        if (info.status == 200) {
            fprintf(stderr, "[SIP] Un-registered successfully\n");
        } else {
            fprintf(stderr, "[SIP] Un-registered unsuccessfully, continue anyway...\n");
        }

        g_sip_registered = 0;
        return;
    }

    if (info.status == 200) {
        fprintf(stderr, "[SIP] Registered successfully\n");
        g_sip_registered = 1;
        g_sip_registering = 0;
    } else {
        fprintf(stderr, "[SIP] Registration failed: %d (%.*s)\n",
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
        fprintf(stderr, "[SIP] Call %d disconnected\n", call_id);
        struct thread_timer_t *t = &g_call_map[call_id].timer;
        pthread_mutex_lock(&t->mutex);
        t->cancelled = true;
        t->milliseconds_left = 0;
        pthread_mutex_unlock(&t->mutex);
    } else if (ci.state == PJSIP_INV_STATE_CALLING) {
        fprintf(stderr, "[SIP] Call %d started\n", call_id);

        // We now are ringing, queue a timer to stop it after the specified timeout!
        struct thread_timer_t *t = &g_call_map[call_id].timer;
        pthread_mutex_lock(&t->mutex);
        t->cancelled = false;
        t->milliseconds_left = 1000 * call_timeout_seconds;
        pthread_mutex_unlock(&t->mutex);

    } else if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        fprintf(stderr, "[SIP] Call confirmed\n");
    } else if (ci.state == PJSIP_INV_STATE_CONNECTING) {
        fprintf(stderr, "[SIP] Call connecting\n");
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
    log_cfg.console_level = run_cfg->log_level;
    log_cfg.level = run_cfg->log_level;

    pjsua_media_config media_cfg;
    pjsua_media_config_default(&media_cfg);

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
        fprintf(stderr, "Error creating transport for SIP\n");
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
        fprintf(stderr, "SIP Account add failed\n");
        return RC_FAILURE;
    }

    fprintf(stderr, "SIP TCP client running...\n");
    return RC_OK;
}

return_code_t initialize_sip_client(struct sip_config *cfg)
{
    call_timeout_seconds = cfg->call_timeout_seconds;
    sip_dest = cfg->callee_uri;
    int rc;

    for (int i = 0; i < PJSUA_MAX_CALLS; i++) {
        struct thread_timer_t* t = &g_call_map[i].timer;
        t->seconds = call_timeout_seconds;
        t->callback = stop_call;
        t->call_id = i;
        t->cancelled = true;
        t->milliseconds_left = 0;
        rc = pthread_mutex_init(&t->mutex, NULL);
        if (rc < 0)
            return RC_FAILURE;

        rc = threaded_timer_create(t);
        if (rc < 0)
            return RC_FAILURE;
    }

    /* Initialize PJSIP with our runtime configuration */
    rc = pjsip_init_app(cfg);
    if (rc != RC_OK)
        return RC_FAILURE;

    /* Start PJSIP threads - one for monitoring the registration
     * and another one for tracking call requests from a queue.
     */
    rc = pthread_create(&sip_call_thread, NULL, pjsip_call_thread, NULL);
    if (rc != RC_OK)
        return RC_FAILURE;

    rc = pthread_create(&sip_registration_thread, NULL, pjsip_registration_thread, NULL);
    if (rc != RC_OK)
        return RC_FAILURE;

    return RC_OK;
}

void stop_sip_client()
{
    for (int i = 0; i < PJSUA_MAX_CALLS; i++) {
        struct thread_timer_t* t = &g_call_map[i].timer;
        pthread_mutex_lock(&t->mutex);
    }

    action_t a = { .type = ACTION_HANGUP_ALL, .call_id = PJSUA_INVALID_ID };
    queue_push(&g_sip_queue, a);

    pthread_join(sip_call_thread, NULL);

    fprintf(stderr, "[SIP] Call handling thread stopped.\n");

    /* Stop registration thread (registration attempts) completely from 
       this point onwards */
    g_sip_stop_registration_thread = true;

    pjsua_acc_set_registration(acc_id, PJ_FALSE);

    unsigned timeout_seconds = 0;
    while (g_sip_registered && timeout_seconds < 5) {
        sleep(1);
        timeout_seconds++;
    }

    if (g_sip_registered)
        fprintf(stderr, "[SIP] Unregistering failed, continue anyway...\n");

    /* 
     * Now that we stopped all calls and the handling thread, we can tear down the
     * registration thread as well.
     */

    pthread_join(sip_registration_thread, NULL);

    fprintf(stderr, "[SIP] Registration thread stopped.\n");

    g_sip_stop_all_threads = true;

    for (int i = 0; i < PJSUA_MAX_CALLS; i++) {
        struct thread_timer_t* t = &g_call_map[i].timer;
        pthread_mutex_unlock(&t->mutex);
        pthread_mutex_destroy(&t->mutex);
    }

    pjsua_destroy();

    fprintf(stderr, "[SIP] Shutdown Complete.\n");
}
