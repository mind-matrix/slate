#!/bin/bash -e

install -m 644 files/slate.deb "${ROOTFS_DIR}/tmp/slate.deb"
printf 'SLATE_BUILD_TYPE=%s\n' "${SLATE_BUILD_TYPE}" > "${ROOTFS_DIR}/etc/slate-release"

on_chroot <<EOF
apt-get install -y --no-install-recommends /tmp/slate.deb
rm -f /tmp/slate.deb
EOF
