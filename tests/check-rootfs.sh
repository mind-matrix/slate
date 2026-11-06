#!/bin/bash
set -uo pipefail

usage() {
	echo "usage: $0 <rootfs-dir> dev|release" >&2
	exit 2
}

[ $# -eq 2 ] || usage
ROOTFS="$1"
VARIANT="$2"
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

has_user() { grep -q "^$1:" "$ROOTFS/etc/passwd"; }
hostname_is() { [ "$(cat "$ROOTFS/etc/hostname")" = "$1" ]; }
ssh_enabled() { compgen -G "$ROOTFS/etc/systemd/system/*.wants/ssh.*" > /dev/null; }
ssh_disabled() { ! ssh_enabled; }
cloud_init_absent() { [ ! -d "$ROOTFS/etc/cloud" ]; }

check "user marvin exists" has_user marvin
check "hostname is slate" hostname_is slate
check "cloud-init is not installed" cloud_init_absent
case "$VARIANT" in
	dev) check "ssh is enabled" ssh_enabled ;;
	release) check "ssh is disabled" ssh_disabled ;;
	*) usage ;;
esac

[ "$failures" -eq 0 ]
