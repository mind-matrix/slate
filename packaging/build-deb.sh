#!/bin/bash
set -euo pipefail

if [ $# -ne 2 ]; then
	echo "usage: $0 <slate-shell binary> <output dir>" >&2
	exit 2
fi

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGING="$SOURCE_DIR/packaging"
BINARY="$1"
OUT_DIR="$2"
VERSION="$(cat "$SOURCE_DIR/VERSION")"
COMMIT="${SLATE_COMMIT:-$(git -c safe.directory='*' -C "$SOURCE_DIR" rev-parse --short HEAD)}"
PUBLIC_KEY="${SLATE_UPDATE_PUBKEY:-$PACKAGING/update.pub}"
NOTES="$SOURCE_DIR/releases/$VERSION.md"

for required in "$BINARY" "$PUBLIC_KEY" "$NOTES"; do
	if [ ! -f "$required" ]; then
		echo "missing $required" >&2
		exit 1
	fi
done

ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

cp -a "$PACKAGING/root/." "$ROOT/"
find "$ROOT" -type f -exec chmod 644 {} +
install -D -m 755 "$BINARY" "$ROOT/usr/bin/slate-shell"
install -D -m 644 "$PUBLIC_KEY" "$ROOT/usr/share/slate/update.pub"
sed -n 's/^- //p' "$NOTES" > "$ROOT/usr/share/slate/notes"
printf 'SLATE_VERSION=%s\nSLATE_COMMIT=%s\nSLATE_CHANNEL=release\n' "$VERSION" "$COMMIT" > "$ROOT/usr/lib/slate/release"
chmod 755 "$ROOT/usr/lib/slate/slate-update" "$ROOT/usr/lib/slate/slate-apps" "$ROOT/usr/lib/slate/slate-touch"
find "$ROOT" -type d -exec chmod 755 {} +

mkdir -p "$ROOT/DEBIAN"
sed "s/@VERSION@/$VERSION/" "$PACKAGING/control" > "$ROOT/DEBIAN/control"
install -m 755 "$PACKAGING/postinst" "$PACKAGING/postrm" "$ROOT/DEBIAN/"
install -m 644 "$PACKAGING/triggers" "$ROOT/DEBIAN/"

mkdir -p "$OUT_DIR"
dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$OUT_DIR/slate_${VERSION}_arm64.deb" > /dev/null
echo "$OUT_DIR/slate_${VERSION}_arm64.deb"
