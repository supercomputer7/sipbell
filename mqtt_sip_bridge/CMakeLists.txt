cmake_minimum_required(VERSION 3.15)
project(DoorbellMQTT2SIP)

set(CMAKE_C_STANDARD 99)
include_directories(/usr/local/include)
include_directories(/usr/include)
include_directories(include)

link_directories(/usr/local/lib)

add_executable(doorbell_mqtt_sip_bridge bridge.c args.c
                src/mqtt.c src/sip.c src/sip_queue.c src/timer.c)
target_link_libraries(doorbell_mqtt_sip_bridge
    pjsua pjsip-ua pjsip-simple pjsip
    pjmedia pjmedia-codec pjlib-util pj
    m pthread paho-mqtt3a
)

add_compile_definitions(PJ_HAS_LIMITS_H=1)

# Remove this when debug messages are needed for PJLIB
add_compile_definitions(DISABLE_PJLIB_DEBUG_MESSAGES=1)
