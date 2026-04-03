# MQTT-to-SIP bridge

## Overview

`mqtt_sip_bridge` is a lightweight C application that listens to an MQTT
topic and initiates SIP calls when a specific event occurs (e.g., a doorbell
press).

It is designed for minimal dependencies and works on Linux using PJSIP for
SIP signaling and Eclipse Paho MQTT for messaging.

## Features

 - Subscribes to an MQTT topic and reacts to messages
 - Initiates or skips SIP calls based on configuration
 - TCP or UDP SIP transport support
 - Configurable call timeout and retry logic
 - Timer-based management of call lifecycles
 - Thread-safe and race-condition aware
 - Minimal audio handling (signaling only, no audio processing required)
 - Graceful shutdown and resource cleanup

## Build Instructions

 - Requires PJSIP and Eclipse Paho MQTT C library
 - Uses CMake for build configuration
 - Supports Linux and standard POSIX threads

## Prerequisites
 
 - Linux (Ubuntu/Debian recommended)
 - gcc, make, cmake, git

Optional: libssl-dev if you want TLS support

Install build tools on Ubuntu/Debian:
```sh
sudo apt-get update
sudo apt-get install build-essential gcc cmake git libssl-dev
```

# Build and Install Eclipse Paho C Library

I used version 1.3.16, so it's safest to compile it:
```sh
git clone --branch v1.3.16 --single-branch https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
```

## Configure CMake (disable SSL for simple TCP)

```sh
cmake -B build -S . -DPAHO_WITH_SSL=OFF
```

## Build and install

```sh
cmake --build build/
sudo cmake --install build/
```

# Build PJSIP

These are roughly the commands to build PJSIP on your system:
```sh
git clone https://github.com/pjsip/pjproject.git

./configure --prefix=/usr/local \
            --enable-shared \
            --disable-speex-codec \
            --disable-speex-aec \
            --disable-gsm-codec \
	        --disable-libsrtp \
            --disable-resample \
            --disable-video \
            --disable-opencore-amr

make dep
make -j$(nproc)

sudo make install
```

As of writing this document, the project's last commit is at `8e0bcc6da2a33efb8792b1b642461a6797902ec3`
(March 13 2026) - this commit can be used, if anything breaks in the future.

# Building the program

## Update linker cache

```sh
sudo ldconfig
```

By default, headers are installed in /usr/local/include and libraries in /usr/local/lib
to avoid mixing system libraries and user-compiled ones together.

## Compile the program

You should be able to compile it now by doing:
```sh
mkdir Build 
cd Build
cmake ..
make
```

If you get libpaho-mqtt3a.so.1 not found, you can run:

```sh
sudo sh -c 'echo "/usr/local/lib" > /etc/ld.so.conf.d/usr-local-lib.conf'
sudo ldconfig
```

Or temporarily set the library path:
```sh
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./doorbell_mqtt_sip_bridge
```

## Running!

To run this, use this for SIP UDP transport:
```sh
./doorbell_mqtt_sip_bridge -R SIP_HOST -U doorbell -S SIP_PASS -b MQTT_BROKER -t home/doorbell/ring -u MQTT_USER -p MQTT_PASS -C joe
```

or for SIP TCP transport (add `-T` flag):
```sh
./doorbell_mqtt_sip_bridge -R SIP_HOST -U doorbell -S SIP_PASS -b MQTT_BROKER -t home/doorbell/ring -u MQTT_USER -p MQTT_PASS -C joe -T
```

See help (`-h`) for details on arguments being passed to the program.

## Troubleshooting

### Library not found at runtime
Make sure /usr/local/lib is in the linker path:

```sh
sudo ldconfig
```

Or temporarily:

```sh
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### SSL/TLS issues
If your broker requires SSL, enable it during Paho build:

```sh
cmake -B build -S . -DPAHO_WITH_SSL=ON
```

And configure MQTTClient_SSLOptions accordingly.

## PBX configuration

I use Asterisk as it is a well-tested open-source PBX product. I configured my
dialplan to look like this:

```
exten => SOME_EXTENSION_NUMBER,1,NoOp
same => n,Queue(doorbell,,,,400)
same => n,Hangup
```

Ensure you don't do something like this (adding Answer directive in between):
```
exten => SOME_EXTENSION_NUMBER,1,NoOp
same => n,Answer()
same => n,Queue(doorbell,,,,400)
same => n,Hangup
```

As it will probably try to negotiate audio codecs immediately, and the PBX will just hangup
the call immediately with the bridge.

Don't forget to actually define a queue in queues.conf. It should look like this:
```
[doorbell]
strategy = ringall
member = PJSIP/user1
member = PJSIP/user2
member = PJSIP/user3
member = PJSIP/user4
```

## Testing without a button click

Since this is all driven by MQTT messages, you install `mosquitto` on
your Linux machine and do this command:

```sh
mosquitto_pub -h HOST -u USER -P PASS -t home/doorbell/ring -m RING
```

## Licenses & Why no prebuilt binaries

This project is licensed under the MIT license. See LICENSE for more details.

This project depends on:
- PJSIP (GPLv2)
- Eclipse Paho MQTT C client

I don't intend to share any prebuilt binaries of the project, to avoid
redistribution clauses in licenses of the libraries I use - this is also
true of the Dockerfile which builds these dependencies locally only.

## No manual page

There's no manual page - this is intentional. I worked for about 5 years
in the SerenityOS project and came to a concise conclusion that groff
is a terrible typesetting system and I don't think I am alone with that
idea. Sadly, we are kinda stuck with it, but since this program is **very**
simple, the help menu can be a good guide for anyone which wants to use
this bridge.
