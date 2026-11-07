#!/bin/bash -e

install -m 755 files/slate-shell "${ROOTFS_DIR}/usr/local/bin/slate-shell"
install -m 644 files/slate-shell.service "${ROOTFS_DIR}/etc/systemd/system/slate-shell.service"

on_chroot <<EOF
systemctl disable getty@tty1.service
systemctl enable slate-shell.service
EOF
