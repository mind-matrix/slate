#!/bin/bash -e

install -D -m 644 files/bluez.conf "${ROOTFS_DIR}/etc/wireplumber/wireplumber.conf.d/50-slate-bluez.conf"
