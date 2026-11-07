#!/bin/bash
set -euo pipefail

if [ $# -ne 1 ]; then
	echo "usage: $0 <tag, e.g. v2026.11.20>" >&2
	exit 2
fi

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="${SLATE_REPO:-mind-matrix/slate}"
SECRET_KEY="${SLATE_SIGNING_KEY:-$HOME/.minisign/slate.key}"
PUBLIC_KEY="$SOURCE_DIR/packaging/update.pub"
TAG="$1"
VERSION="${TAG#v}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

gh release download "$TAG" -R "$REPO" -p manifest.json -p '*.deb' -D "$WORK"

manifest="$WORK/manifest.json"
file="$(jq -er .package.file "$manifest")"
[ "$(jq -er .version "$manifest")" = "$VERSION" ] || { echo "manifest version does not match $TAG" >&2; exit 1; }
[ "$(jq -er .channel "$manifest")" = release ] || { echo "manifest is not on the release channel" >&2; exit 1; }
[ "$(jq -er .package.url "$manifest")" = "https://github.com/$REPO/releases/download/$TAG/$file" ] ||
	{ echo "package URL does not point at $TAG" >&2; exit 1; }
[ "$(jq -er .package.sha256 "$manifest")" = "$(sha256sum "$WORK/$file" | cut -d' ' -f1)" ] ||
	{ echo "package hash does not match the manifest" >&2; exit 1; }
[ "$(jq -er .package.size "$manifest")" = "$(stat -c %s "$WORK/$file")" ] ||
	{ echo "package size does not match the manifest" >&2; exit 1; }

echo "Signing Slate $VERSION:"
jq -r '.notes[] | "  - " + .' "$manifest"
minisign -S -s "$SECRET_KEY" -m "$manifest" -t "Slate $VERSION"
minisign -Vq -p "$PUBLIC_KEY" -m "$manifest"

gh release upload "$TAG" "$manifest.minisig" -R "$REPO" --clobber
gh release edit "$TAG" -R "$REPO" --draft=false --latest
echo "Published $TAG"
