#!/bin/bash -e

if [ -z "${SLATE_WIFI_SSID:-}" ]; then
	exit 0
fi

profile="${ROOTFS_DIR}/etc/NetworkManager/system-connections/slate-dev.nmconnection"
install -d -m 700 "$(dirname "${profile}")"
cat > "${profile}" <<EOF
[connection]
id=slate-dev
type=wifi
autoconnect=true

[wifi]
mode=infrastructure
ssid=${SLATE_WIFI_SSID}

[wifi-security]
key-mgmt=wpa-psk
psk=${SLATE_WIFI_PASSWORD}

[ipv4]
method=auto

[ipv6]
method=auto
EOF
chmod 600 "${profile}"
