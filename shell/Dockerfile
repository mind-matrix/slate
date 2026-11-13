FROM debian:trixie
# The shell links Raspberry Pi's wlroots build, which differs from Debian's, so headers and
# libraries come from Raspberry Pi's archive like they do on the device.
COPY raspberrypi-archive-keyring.pgp /usr/share/keyrings/raspberrypi-archive-keyring.pgp
RUN echo "deb [signed-by=/usr/share/keyrings/raspberrypi-archive-keyring.pgp] http://archive.raspberrypi.com/debian/ trixie main" > /etc/apt/sources.list.d/raspi.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake pkg-config libdrm-dev libsystemd-dev libvterm-dev ca-certificates \
       libwlroots-0.18-dev libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev libpixman-1-dev libsodium-dev \
    && rm -rf /var/lib/apt/lists/*
