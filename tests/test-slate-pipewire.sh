#!/bin/bash
set -uo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HELPER="$SOURCE_DIR/packaging/root/usr/lib/slate/slate-pipewire-conf"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0

check() {
	local description="$1"
	shift
	if "$@"; then
		echo "PASS  $description"
	else
		echo "FAIL  $description"
		failures=$((failures + 1))
	fi
}

run() {
	SLATE_PIPEWIRE_SOURCE="$WORK/source.conf" SLATE_PIPEWIRE_TARGET="$WORK/etc/pipewire.conf" sh "$HELPER" 2> "$WORK/stderr"
}

cat > "$WORK/source.conf" <<'EOF'
context.modules = [
    { name = libpipewire-module-protocol-native
        args = {
            # List of server Unix sockets, and optionally permissions
            #sockets = [ { name = "pipewire-0" }, { name = "pipewire-0-manager" } ]
        }
    }
    { name = libpipewire-module-access
        args = {
            # Socket-specific access permissions
            #access.socket = { pipewire-0 = "default", pipewire-0-manager = "unrestricted" }
        }
    }
]
EOF
run
TARGET="$WORK/etc/pipewire.conf"
check "the assistant socket is added" grep -q '^            sockets = \[ { name = "pipewire-0" }, { name = "pipewire-0-manager" }, { name = "/run/slate-audio/pipewire-0" mode = "0660" } \]$' "$TARGET"
check "the usual sockets keep their access" grep -q 'access.socket = { pipewire-0 = "default", pipewire-0-manager = "unrestricted", "/run/slate-audio/pipewire-0" = "restricted" }' "$TARGET"
check "the rest of the file is unchanged" test "$(grep -vc slate-audio "$TARGET")" = "$(grep -vc -e '#sockets' -e '#access.socket' "$WORK/source.conf")"

echo "previous" > "$TARGET"
sed -i 's/#sockets/#socket_list/' "$WORK/source.conf"
run
check "a changed source leaves the current file alone" grep -qx previous "$TARGET"
check "a changed source is reported" grep -q "changed shape" "$WORK/stderr"
check "no partial file is left behind" test ! -e "$TARGET.new"

rm -f "$WORK/source.conf"
check "a missing source is not an error" run

if [ "$failures" -gt 0 ]; then
	echo "$failures PipeWire config check(s) failed"
	exit 1
fi
echo
echo "All PipeWire config checks passed"
