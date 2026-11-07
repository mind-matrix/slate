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
boot_config_has() { grep -qx "$1" "$ROOTFS/boot/firmware/config.txt"; }
unit_enabled() { compgen -G "$ROOTFS/etc/systemd/system/*.wants/$1" > /dev/null; }
unit_disabled() { ! unit_enabled "$1"; }
owned_by() { [ "$(stat -c %u "$ROOTFS$2")" = "$(awk -F: -v u="$1" '$1 == u { print $3 }' "$ROOTFS/etc/passwd")" ]; }
in_group() { grep -qE "^$1:[^:]*:[^:]*:(.*,)?$2(,|$)" "$ROOTFS/etc/group"; }

check "user marvin exists" has_user marvin
check "hostname is slate" hostname_is slate
check "cloud-init is not installed" cloud_init_absent
for package in pipewire wireplumber libspa-0.2-bluetooth libvterm0; do
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
check "polkit rule for marvin" file_exists /usr/share/polkit-1/rules.d/50-slate.rules
check "marvin session lingers" file_exists /var/lib/systemd/linger/marvin
check "marvin memlock is unlimited" file_exists /etc/security/limits.d/50-slate-memlock.conf
check "marvin is in bluetooth group" in_group bluetooth marvin
check "bluetooth audio works without a seat" file_exists /usr/share/wireplumber/wireplumber.conf.d/50-slate-bluez.conf
check "camera auto-detect is off" boot_config_has camera_auto_detect=0
check "display auto-detect is on" boot_config_has display_auto_detect=1
check "touch display 2 is landscape" boot_config_has dtoverlay=vc4-kms-dsi-ili9881-7inch,rotation=90
check "the slate package is installed" installed slate
check "slate-shell is installed" test -x "$ROOTFS/usr/bin/slate-shell"
check "slate-shell starts at boot" unit_enabled slate-shell.service
check "slate-shell can reach the audio session" grep -qx "Environment=XDG_RUNTIME_DIR=/run/user/1000" "$ROOTFS/usr/lib/systemd/system/slate-shell.service"
check "build type is recorded" grep -qx "SLATE_BUILD_TYPE=$VARIANT" "$ROOTFS/etc/slate-release"
check "tty1 login is replaced by the shell" unit_disabled getty@tty1.service
check "the installed version is recorded" grep -q "^SLATE_VERSION=" "$ROOTFS/usr/lib/slate/release"
check "updates are verified with the Slate key" file_exists /usr/share/slate/update.pub
check "the updater is installed" test -x "$ROOTFS/usr/lib/slate/slate-update"
check "interrupted updates are recovered at boot" unit_enabled slate-update-recover.service
check "the updater can verify and download" installed minisign
check "no stale shell copy in /usr/local" test ! -e "$ROOTFS/usr/local/bin/slate-shell"
case "$VARIANT" in
	dev)
		check "marvin can redeploy the shell" owned_by marvin /usr/bin/slate-shell
		check "marvin can restart the shell" file_exists /etc/polkit-1/rules.d/50-slate-shell-deploy.rules
		check "ssh is enabled" ssh_enabled
		check "evtest is installed" installed evtest
		check "slate-memreport is executable" test -x "$ROOTFS/usr/local/bin/slate-memreport"
		;;
	release)
		check "ssh is disabled" ssh_disabled
		check "evtest is not installed" not_installed evtest
		check "shell binary is root-owned" owned_by root /usr/bin/slate-shell
		;;
	*) usage ;;
esac

[ "$failures" -eq 0 ]
