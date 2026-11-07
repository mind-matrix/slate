#!/bin/bash -e

install -m 755 files/slate-shell "${ROOTFS_DIR}/usr/local/bin/slate-shell"
install -m 644 files/slate-shell.service "${ROOTFS_DIR}/etc/systemd/system/slate-shell.service"
printf 'SLATE_BUILD_TYPE=%s\nSLATE_VERSION="%s"\n' "${SLATE_BUILD_TYPE}" "${SLATE_VERSION}" > "${ROOTFS_DIR}/etc/slate-release"

on_chroot <<EOF
systemctl disable getty@tty1.service
systemctl enable slate-shell.service
EOF
