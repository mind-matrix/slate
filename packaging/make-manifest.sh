#!/bin/bash
set -euo pipefail

if [ $# -ne 2 ]; then
	echo "usage: $0 <package.deb> <download base url>" >&2
	exit 2
fi

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE="$1"
BASE_URL="$2"
VERSION="$(cat "$SOURCE_DIR/VERSION")"
FILE="$(basename "$PACKAGE")"

jq -n \
	--arg version "$VERSION" \
	--arg min_version "$(cat "$SOURCE_DIR/packaging/min-version")" \
	--arg file "$FILE" \
	--arg url "$BASE_URL/$FILE" \
	--arg sha256 "$(sha256sum "$PACKAGE" | cut -d' ' -f1)" \
	--argjson size "$(stat -c %s "$PACKAGE")" \
	--rawfile remove "$SOURCE_DIR/packaging/obsolete-packages" \
	--rawfile notes "$SOURCE_DIR/releases/$VERSION.md" \
	'{
		version: $version,
		channel: "release",
		min_version: $min_version,
		package: { file: $file, url: $url, sha256: $sha256, size: $size },
		remove: ($remove | split("\n") | map(select(length > 0))),
		notes: ($notes | split("\n") | map(select(startswith("- ")) | .[2:]))
	}'
