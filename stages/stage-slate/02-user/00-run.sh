#!/bin/bash -e

install -D -m 644 files/memlock-user-manager.conf "${ROOTFS_DIR}/etc/systemd/system/user@1000.service.d/50-slate-memlock.conf"
install -D -m 644 files/memlock.conf "${ROOTFS_DIR}/etc/systemd/user.conf.d/50-slate-memlock.conf"
install -D -m 644 files/memlock-limits.conf "${ROOTFS_DIR}/etc/security/limits.d/50-slate-memlock.conf"
install -D -m 644 /dev/null "${ROOTFS_DIR}/var/lib/systemd/linger/${FIRST_USER_NAME}"

on_chroot <<EOF
usermod -aG bluetooth "${FIRST_USER_NAME}"
EOF
