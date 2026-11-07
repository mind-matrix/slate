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
installed() { [ -f "$ROOTFS/var/lib/dpkg/info/$1.list" ] || [ -f "$ROOTFS/var/lib/dpkg/info/$1:arm64.list" ]; }
not_installed() { ! installed "$1"; }
masked() { [ "$(readlink "$ROOTFS/etc/systemd/system/$1")" = /dev/null ]; }
file_exists() { [ -f "$ROOTFS$1" ]; }
in_group() { grep -qE "^$1:[^:]*:[^:]*:(.*,)?$2(,|$)" "$ROOTFS/etc/group"; }

check "user marvin exists" has_user marvin
check "hostname is slate" hostname_is slate
check "cloud-init is not installed" cloud_init_absent
for package in pipewire wireplumber libspa-0.2-bluetooth; do
	check "$package is installed" installed "$package"
done
for package in cloud-init udisks2 cron man-db avahi-daemon; do
	check "$package is not installed" not_installed "$package"
done
for timer in apt-daily.timer apt-daily-upgrade.timer e2scrub_all.timer; do
	check "$timer is masked" masked "$timer"
done
check "journald is volatile" file_exists /etc/systemd/journald.conf.d/50-slate.conf
check "swap uses zram" file_exists /etc/rpi/swap.conf.d/50-slate.conf
check "polkit rule for marvin" file_exists /etc/polkit-1/rules.d/50-slate.rules
check "marvin session lingers" file_exists /var/lib/systemd/linger/marvin
check "marvin memlock is unlimited" file_exists /etc/security/limits.d/50-slate-memlock.conf
check "marvin is in bluetooth group" in_group bluetooth marvin
case "$VARIANT" in
	dev) check "ssh is enabled" ssh_enabled ;;
	release) check "ssh is disabled" ssh_disabled ;;
	*) usage ;;
esac

[ "$failures" -eq 0 ]
