#!/bin/bash
set -euo pipefail

if [ $# -ne 1 ]; then
	echo "usage: $0 <output dir>" >&2
	exit 2
fi

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${HELLO_VERSION:-0.1.0}"
ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

install -D -m 755 "$APP_DIR/run" "$ROOT/usr/lib/slate-apps/hello/run"
install -D -m 644 "$APP_DIR/hello.app" "$ROOT/usr/share/slate/apps/hello.app"
mkdir -p "$ROOT/DEBIAN"
cat > "$ROOT/DEBIAN/control" <<CONTROL
Package: slate-app-hello
Version: $VERSION
Architecture: arm64
Maintainer: Slate <noreply@example.com>
Description: Test app for the Slate app platform
CONTROL
mkdir -p "$1"
dpkg-deb --root-owner-group --build "$ROOT" "$1/slate-app-hello_${VERSION}_arm64.deb" >/dev/null
echo "$1/slate-app-hello_${VERSION}_arm64.deb"
