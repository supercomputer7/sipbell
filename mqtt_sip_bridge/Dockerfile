FROM debian:bookworm-slim

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpaho-mqtt1.3 \
    libpaho-mqtt-dev \
    pkg-config \
    python3 \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Download and build PJSIP
WORKDIR /opt
RUN git clone -b 2.16 https://github.com/pjsip/pjproject.git
WORKDIR /opt/pjproject
RUN ./configure --prefix=/usr/local --enable-shared \
            --disable-speex-codec \
            --disable-speex-aec \
            --disable-gsm-codec \
	        --disable-libsrtp \
            --disable-resample \
            --disable-video \
            --disable-opencore-amr \
            --disable-sound && make dep && make && make install

# Copy your source code
WORKDIR /app
COPY . /app

# Build
RUN cmake -B build && cmake --build build -- -j$(nproc)

# Runtime
CMD ["./build/doorbell_mqtt_sip_bridge"]
