/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "MQTTAsync.h"
#include "defs.h"

#include "mqtt.h"
#include "sip_queue.h"

static MQTTAsync s_client;

struct mqtt_config *s_cfg;

extern volatile int g_call_request;
extern action_queue_t g_sip_queue;

volatile bool g_mqtt_connected = false;
volatile bool s_subscribed = 0;

int mqtt_message_callback(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    if (strcmp(topicName, s_cfg->topic) == 0) {
        char* payload = message->payload;
        if (strncmp(payload, "RING", message->payloadlen) == 0) {
            action_t a = { .type = ACTION_CALL };
            queue_push(&g_sip_queue, a);
        }
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void subscribe_success(void* context, MQTTAsync_successData* response) {
    s_subscribed = 1;
    fprintf(stderr, "[MQTT] Subscription successful.\n");
}

void subscribe_failure(void* context, MQTTAsync_failureData* response) {
    s_subscribed = 0;
    int rc = response ? response->code : -1;
    fprintf(stderr, "[MQTT] Subscription failed, rc=%d\n", rc);
}

int mqtt_subscribe_safe(MQTTAsync client, const char* topic, int qos) {
    if (!g_mqtt_connected) {
        fprintf(stderr, "[MQTT] Cannot subscribe, client not connected.\n");
        return 0;
    }

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = subscribe_success;
    opts.onFailure = subscribe_failure;
    opts.context = client;

    int rc = MQTTAsync_subscribe(client, topic, qos, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        fprintf(stderr, "[MQTT] Failed to schedule subscribe, rc=%d\n", rc);
        return 0;
    }

    return 1; // scheduled successfully
}

void on_mqtt_connect(void* context, MQTTAsync_successData* response) {
    MQTTAsync client = (MQTTAsync)context;

    fprintf(stderr, "[MQTT] Connected to broker\n");
    g_mqtt_connected = true;

    // Always try to subscribe whenever we connect/reconnect
    if (mqtt_subscribe_safe((MQTTAsync)context, s_cfg->topic, 1)) {
        fprintf(stderr, "[MQTT] Subscribe request scheduled.\n");
    }
}

void on_mqtt_disconnect(void* context, MQTTAsync_failureData* response) {
    fprintf(stderr, "MQTT: connection failed, rc=%d\n", response ? response->code : -1);
    g_mqtt_connected = false;
}

void connection_lost(void *context, char *cause) {
    fprintf(stderr, "MQTT connection lost: %s\n", cause ? cause : "unknown");
    g_mqtt_connected = false;
}

return_code_t initailize_mqtt_client(struct mqtt_config *cfg)
{
    s_cfg = cfg;
    // --- Initialize MQTT ---
    MQTTAsync_create(&s_client, cfg->broker, cfg->client_string_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTAsync_setCallbacks(s_client, s_client, connection_lost, mqtt_message_callback, NULL);

    return RC_OK;
}

static return_code_t wait_five_seconds(volatile bool *flag)
{
    // Wait up to 5 seconds for connection
    int waited_ms = 0;
    while (!*flag && waited_ms < 5000) {
        struct timespec req;
        req.tv_sec = 0;               // seconds
        req.tv_nsec = 100 * 1000000;  // 100 milliseconds = 100,000,000 nanoseconds

        if (nanosleep(&req, NULL) == -1) {
            perror("nanosleep");
            return RC_FAILURE;
        }
        waited_ms += 100;
    }
    return RC_OK;
}

static return_code_t mqtt_client_connect(int max_retries) {
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = on_mqtt_connect;
    conn_opts.onFailure = on_mqtt_disconnect;
    conn_opts.context = s_client;
    if (strnlen(s_cfg->user, sizeof(s_cfg->user)) != 0)
        conn_opts.username = s_cfg->user;

    if (strnlen(s_cfg->pass, sizeof(s_cfg->pass)) != 0)
        conn_opts.password = s_cfg->pass;

    int attempt = 0;

    while (attempt < max_retries) {
        if (!g_mqtt_connected) {
            int rc = MQTTAsync_connect(s_client, &conn_opts);
            if (rc != MQTTASYNC_SUCCESS) {
                fprintf(stderr, "[MQTT] Connect attempt %d failed to start, rc=%d\n", attempt + 1, rc);
            } else {
                fprintf(stderr, "[MQTT] Connect attempt %d scheduled...\n", attempt + 1);
            }

            rc = wait_five_seconds(&g_mqtt_connected);
            if (rc != RC_OK)
                return RC_FAILURE;

            if (g_mqtt_connected)
                return RC_OK; // success
                
            attempt++;
            fprintf(stderr, "[MQTT] Connect attempt %d failed, retrying in %d seconds...\n", attempt, MQTT_RETRY_DELAY);
            sleep(MQTT_RETRY_DELAY);
        }
    }

    fprintf(stderr, "[MQTT] Failed to connect after %d attempts\n", max_retries);
    return RC_FAILURE; // success
}

return_code_t mqtt_client_connect_and_subscribe(int max_retries)
{
    int rc = mqtt_client_connect(max_retries);
    if (rc != RC_OK)
        return RC_FAILURE;
    // We should automatically try to connect to the topic, so let's wait
    // for max_retries as well here -

    unsigned attempt = 0;
    while (attempt < max_retries) {
        // In this case, we already had a connection, so let's fail here
        if (!g_mqtt_connected) {
            return RC_FAILURE;
        }

        rc = wait_five_seconds(&s_subscribed);
        if (rc != RC_OK)
            return RC_FAILURE;

        if (s_subscribed)
            return RC_OK; // success
            
        attempt++;
        fprintf(stderr, "[MQTT] Subscribe attempt %d failed, retrying in %d seconds...\n", attempt, MQTT_RETRY_DELAY);
        sleep(MQTT_RETRY_DELAY);
    }

    return RC_FAILURE;
}

bool mqtt_client_connected()
{
    return g_mqtt_connected;
}

void stop_mqtt_client()
{
    MQTTAsync_destroy(&s_client);
}
