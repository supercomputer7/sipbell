/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sip.h"
#include "mqtt.h"
#include "defs.h"

extern volatile bool g_mqtt_connected;
extern volatile int g_sip_connected;
extern volatile int g_sip_fatally_shutdown;

extern return_code_t parse_args(int argc, char *argv[], struct sip_config *sip_cfg, struct mqtt_config *mqtt_cfg);

int main(int argc, char* argv[]) {
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

    // We initiate a first call to this function just to initiate a connection
    rc = mqtt_client_connect_and_subscribe(RETRY_MAX);
    if (rc != RC_OK)
        return 1;
    
    while (1) {
        if (!g_mqtt_connected) {
            fprintf(stderr,"[MAIN] MQTT disconnected, trying to reconnect...\n");
            rc = mqtt_client_connect_and_subscribe(RETRY_MAX);
            if (rc != RC_OK) {
                fprintf(stderr,"[MAIN] MQTT disconnected, failed to reconnect...\n");
                exit_code = 1;
                break;
            }
        }

        if (g_sip_fatally_shutdown) {
            fprintf(stderr,"[MAIN] SIP is not connecting, cannot proceed...\n");
            exit_code = 1;
            break;
        }
        sleep(1); // Keep main thread alive
    }

    stop_sip_client();
    stop_mqtt_client();
    return exit_code;
}
