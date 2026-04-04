/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "defs.h"
#include "sip.h"
#include "mqtt.h"

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);

    printf("MQTT options:\n");
    printf("  -b <broker>        MQTT broker connection (default: tcp://localhost:1883)\n");
    printf("  -t <topic>         MQTT topic (default: sip/call)\n");
    printf("  -u <user>          MQTT username (default: empty)\n");
    printf("  -p <pass>          MQTT password (default: empty)\n");
    printf("  -i <client_id>     MQTT client ID string (default: mqtt2sip-bridge)\n");

    printf("\nSIP options:\n");
    printf("  -R <sip_host>                SIP server (default: localhost)\n");
    printf("  -O <sip_local_listen_port>   SIP local listen port (default: 5060)\n");
    printf("  -U <sip_user>                SIP username (default: example)\n");
    printf("  -S <sip_secret>              SIP password\n");
    printf("  -C <callee>                  SIP callee ID (default: joe)\n");
    printf("  -T                    Use TCP transport (default: UDP)\n");
    printf("  -l                    SIP log level in range of 0-4 (default: 0)\n");

    printf("\nCall behavior:\n");
    printf("  -w <seconds>       Call duration before hangup (default: 5 seconds)\n");

    printf("\nGeneral:\n");
    printf("  -h                 Show this help\n");
}

int parse_port(const char *str) {
    char *endptr;
    errno = 0;

    long val = strtol(str, &endptr, 10);

    if (errno != 0 || *endptr != '\0') return -1;
    if (val < 1 || val > 65535) return -1;

    return (int)val;
}

return_code_t parse_args(int argc, char *argv[], struct sip_config *sip_cfg, struct mqtt_config *mqtt_cfg)
{
    char sip_host[128];
    char sip_user[128];
    char sip_callee_id[128];

    memset(&sip_host, 0, sizeof(sip_host));
    memset(&sip_user, 0, sizeof(sip_user));
    memset(&sip_callee_id, 0, sizeof(sip_callee_id));

    // MQTT defaults
    strcpy(mqtt_cfg->broker, "tcp://localhost:1883");
    strcpy(mqtt_cfg->topic, "sip/call");
    strcpy(mqtt_cfg->client_string_id, "mqtt2sip-bridge");

    mqtt_cfg->user[0] = '\0';
    mqtt_cfg->pass[0] = '\0';
    mqtt_cfg->port = 1883;

    // SIP defaults
    strcpy(sip_host, "localhost");
    sip_cfg->port = 5060;

    strcpy(sip_cfg->user, "example");
    sip_cfg->pass[0] = '\0';

    sip_cfg->log_level = 0;
    
    strcpy(sip_cfg->callee_uri, "sip:joe@localhost");
    strcpy(sip_callee_id, "joe");

    sip_cfg->use_tcp_transport = 0;
    sip_cfg->call_timeout_seconds = 10;

    int opt;

    while ((opt = getopt(argc, argv, "b:P:t:u:p:R:O:U:i:l:S:C:Tw:h")) != -1) {
        switch (opt) {
            case 'b': {
                strncpy(mqtt_cfg->broker, optarg, sizeof(mqtt_cfg->broker)-1);
                break;
            }
            case 't': {
                strncpy(mqtt_cfg->topic, optarg, sizeof(mqtt_cfg->topic)-1);
                break;
            }
            case 'u': {
                strncpy(mqtt_cfg->user, optarg, sizeof(mqtt_cfg->user)-1);
                break;
            } 
            case 'p': {
                strncpy(mqtt_cfg->pass, optarg, sizeof(mqtt_cfg->pass)-1);
                break;
            }

            case 'i': {
                strncpy(mqtt_cfg->client_string_id, optarg, sizeof(mqtt_cfg->client_string_id)-1);
                break;
            }

            case 'R': {
                strncpy(sip_host, optarg, sizeof(sip_host)-1);
                break;
            }
            case 'O': {
                sip_cfg->port = parse_port(optarg);
                if (sip_cfg->port < 0) {
                    fprintf(stderr, "Invalid SIP port\n");
                    return RC_FAILURE;
                }
                break;
            }
                
            case 'U': {
                strncpy(sip_cfg->user, optarg, sizeof(sip_cfg->user)-1);
                break;
            }
            case 'S': {
                strncpy(sip_cfg->pass, optarg, sizeof(sip_cfg->pass)-1);
                break;
            }
            case 'C': {
                strncpy(sip_callee_id, optarg, sizeof(sip_callee_id)-1);
                break;
            }
            case 'T': {
                sip_cfg->use_tcp_transport = 1; break;
            }

            case 'w': {
                int val = atoi(optarg);
                if (val <= 0 || val > 255) {
                    fprintf(stderr, "Invalid wait time\n");
                    return RC_FAILURE;
                }
                sip_cfg->call_timeout_seconds = val;
                break;
            }

            case 'l': {
                int val = atoi(optarg);
                if (val < 0 || val > 4) {
                    fprintf(stderr, "Invalid SIP log level\n");
                    return RC_FAILURE;
                }
                sip_cfg->log_level = val;
                break;
            }

            case 'h':
            default:
                print_usage(argv[0]);
                return RC_FAILURE;
        }
    }

    // Finally, apply changes on SIP values
    snprintf(sip_cfg->id, sizeof(sip_cfg->id), "sip:%s@%s", sip_cfg->user, sip_host);
    snprintf(sip_cfg->reg_uri, sizeof(sip_cfg->id), "sip:%s", sip_host);
    if (sip_cfg->use_tcp_transport) {
        snprintf(sip_cfg->callee_uri, sizeof(sip_cfg->callee_uri), "sip:%s@%s;transport=tcp",
                sip_callee_id, sip_host);
    } else {
        snprintf(sip_cfg->callee_uri, sizeof(sip_cfg->callee_uri), "sip:%s@%s",
                sip_callee_id, sip_host);
    }

    return RC_OK;
}
