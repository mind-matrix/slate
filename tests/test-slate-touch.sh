#!/bin/bash
set -uo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
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
	SLATE_TOUCH_DEVICES="$WORK/devices" SLATE_TOUCH_DRIVER="$WORK/driver" SLATE_TOUCH_TRIES=1 \
		bash "$SOURCE_DIR/packaging/root/usr/lib/slate/slate-touch"
}

mkdir -p "$WORK/devices/11-005d" "$WORK/devices/11-0045" "$WORK/devices/1-0020" "$WORK/driver"
echo gt911 > "$WORK/devices/11-005d/name"
echo rpi-panel > "$WORK/devices/11-0045/name"
echo gt911 > "$WORK/devices/1-0020/name"
ln -s "$WORK/driver" "$WORK/devices/1-0020/driver"
run
check "an unbound touch controller is probed again" grep -qx 11-005d "$WORK/driver/bind"
check "other I2C devices are left alone" bash -c "! grep -q 11-0045 '$WORK/driver/bind'"
check "a touch controller that is already working is left alone" bash -c "! grep -q 1-0020 '$WORK/driver/bind'"

rm -f "$WORK/driver/bind"
ln -s "$WORK/driver" "$WORK/devices/11-005d/driver"
run
check "nothing is probed when the touchscreen came up on its own" test ! -e "$WORK/driver/bind"

echo
if [ "$failures" -eq 0 ]; then
	echo "All touchscreen checks passed"
else
	echo "$failures touchscreen checks failed"
	exit 1
fi
