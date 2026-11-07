#!/bin/bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SECRET_KEY="${SLATE_SIGNING_KEY:-$HOME/.minisign/slate.key}"
PUBLIC_KEY="$SOURCE_DIR/packaging/update.pub"

if [ -e "$SECRET_KEY" ]; then
	echo "$SECRET_KEY already exists. Devices trust its public key; replacing it locks them out of updates." >&2
	exit 1
fi

mkdir -p "$(dirname "$SECRET_KEY")"
chmod 700 "$(dirname "$SECRET_KEY")"
minisign -G -p "$PUBLIC_KEY" -s "$SECRET_KEY"
chmod 600 "$SECRET_KEY"
echo
echo "Secret key: $SECRET_KEY (back it up offline; it never goes into git or GitHub)"
echo "Public key: $PUBLIC_KEY (commit this; images built from now on trust it)"
