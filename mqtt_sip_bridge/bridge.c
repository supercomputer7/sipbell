/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "sip.h"
#include "mqtt.h"
#include "defs.h"

extern volatile bool g_mqtt_connected;
extern volatile int g_sip_connected;

extern volatile int g_mqtt_fatally_shutdown;
extern volatile int g_sip_fatally_shutdown;

extern return_code_t parse_args(int argc, char *argv[], struct sip_config *sip_cfg, struct mqtt_config *mqtt_cfg);

volatile sig_atomic_t g_stop = 0;

void handle_sigint(int sig) {
    g_stop = 1;
}

int main(int argc, char* argv[]) {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    int exit_code = 0;

    struct mqtt_config mqtt_cfg;
    struct sip_config sip_cfg;

    memset(&mqtt_cfg, 0, sizeof(struct mqtt_config));
    memset(&sip_cfg, 0, sizeof(struct sip_config));

    if (parse_args(argc, argv, &sip_cfg, &mqtt_cfg)) {
        return 0;
    }

    int rc = initailize_mqtt_client(&mqtt_cfg);
    if (rc != RC_OK)
        return 1;

    rc = initialize_sip_client(&sip_cfg);
    if (rc != RC_OK)
        return 1;

    fprintf(stdout, "Running... Press Ctrl+C to stop\n");
    
    while (!g_stop) {
        if (g_mqtt_fatally_shutdown) {
            fprintf(stderr,"[MAIN] MQTT is not connecting, cannot proceed...\n");
            exit_code = 1;
            break;
        }

        if (g_sip_fatally_shutdown) {
            fprintf(stderr,"[MAIN] SIP is not connecting, cannot proceed...\n");
            exit_code = 1;
            break;
        }

        sleep(1); // Keep main thread alive
    }

    printf("\n");

    stop_sip_client();
    stop_mqtt_client();
    return exit_code;
}
