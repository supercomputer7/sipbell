/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 */

#ifndef __SIP_H__
#define __SIP_H__

#include <stdint.h>
#include <stdbool.h>

#include "defs.h"

struct sip_config {
    char id[128];
    
    char reg_uri[128];
    uint16_t port;

    char user[128];
    char pass[128];
    char callee_uri[128];

    bool use_tcp_transport;

    uint8_t call_timeout_seconds;

    uint8_t log_level;
};

return_code_t initialize_sip_client(struct sip_config *cfg);
void stop_sip_client();

#endif
