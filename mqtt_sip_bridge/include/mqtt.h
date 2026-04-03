/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#ifndef __MQTT_H__
#define __MQTT_H__

#include "defs.h"

#include <stdint.h>

struct mqtt_config {
    char broker[256];
    uint16_t port;
    char topic[128];
    char user[64];
    char pass[64];
};

return_code_t initailize_mqtt_client(struct mqtt_config *cfg);

return_code_t mqtt_client_connect_and_subscribe(int max_retries);

bool mqtt_client_connected();

void stop_mqtt_client();

#endif
