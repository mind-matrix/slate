#!/bin/bash
set -euo pipefail

if [ $# -ne 1 ]; then
	echo "usage: $0 <output dir>" >&2
	exit 2
fi

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTOCOLS="$(cd "$APP_DIR/../../../shell/protocols" && pwd)"
VERSION="${HELLO_VERSION:-0.1.0}"
BUILD_IMAGE="${SLATE_BUILD_IMAGE:-slate-shell-build}"
ROOT="$(mktemp -d)"
UI_OUT="$(mktemp -d)"
trap 'rm -rf "$ROOT" "$UI_OUT"' EXIT

# Compiles ui.c against wayland-client, generating the protocol code it uses.
COMPILE='set -e
P=$(pkg-config --variable=pkgdatadir wayland-protocols)
G=$(mktemp -d)
wayland-scanner client-header $P/stable/xdg-shell/xdg-shell.xml $G/xdg-shell-client-protocol.h
wayland-scanner private-code $P/stable/xdg-shell/xdg-shell.xml $G/xdg-shell-protocol.c
wayland-scanner client-header /protocols/wlr-layer-shell-unstable-v1.xml $G/wlr-layer-shell-unstable-v1-client-protocol.h
wayland-scanner private-code /protocols/wlr-layer-shell-unstable-v1.xml $G/wlr-layer-shell-protocol.c
cc -O2 -Wall -Wextra -I$G /src/ui.c $G/xdg-shell-protocol.c $G/wlr-layer-shell-protocol.c -o /out/hello-ui $(pkg-config --cflags --libs wayland-client)'

if [ "$(uname -m)" = aarch64 ]; then
	mkdir -p /tmp/hello-src && cp "$APP_DIR/ui.c" /tmp/hello-src/
	sh -c "$(echo "$COMPILE" | sed "s#/protocols#$PROTOCOLS#g; s#/src/#/tmp/hello-src/#; s#/out/#$UI_OUT/#")"
else
	docker run --rm --platform linux/arm64 -u "$(id -u):$(id -g)" \
		-v "$APP_DIR:/src:ro" -v "$PROTOCOLS:/protocols:ro" -v "$UI_OUT:/out" "$BUILD_IMAGE" sh -c "$COMPILE"
fi

install -D -m 755 "$APP_DIR/run" "$ROOT/usr/lib/slate-apps/hello/run"
install -D -m 755 "$UI_OUT/hello-ui" "$ROOT/usr/lib/slate-apps/hello/hello-ui"
install -D -m 644 "$APP_DIR/hello.app" "$ROOT/usr/share/slate/apps/hello.app"
mkdir -p "$ROOT/DEBIAN"
cat > "$ROOT/DEBIAN/control" <<CONTROL
Package: slate-app-hello
Version: $VERSION
Architecture: arm64
Maintainer: Slate <noreply@example.com>
Depends: libwayland-client0
Description: Test app for the Slate app platform
CONTROL
mkdir -p "$1"
dpkg-deb --root-owner-group --build "$ROOT" "$1/slate-app-hello_${VERSION}_arm64.deb" >/dev/null
echo "$1/slate-app-hello_${VERSION}_arm64.deb"
