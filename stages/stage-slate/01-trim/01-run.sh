#!/bin/bash -e

install -D -m 644 files/journald.conf "${ROOTFS_DIR}/etc/systemd/journald.conf.d/50-slate.conf"
install -D -m 644 files/swap.conf "${ROOTFS_DIR}/etc/rpi/swap.conf.d/50-slate.conf"
