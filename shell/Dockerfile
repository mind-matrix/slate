FROM debian:trixie
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake pkg-config libdrm-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
