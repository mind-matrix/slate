#!/bin/bash -e

install -m 644 files/50-slate-shell-deploy.rules "${ROOTFS_DIR}/etc/polkit-1/rules.d/50-slate-shell-deploy.rules"

on_chroot <<EOF
chown ${FIRST_USER_NAME}: /usr/bin/slate-shell
EOF
