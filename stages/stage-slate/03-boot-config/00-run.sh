#!/bin/bash -e

config="${ROOTFS_DIR}/boot/firmware/config.txt"
sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' "${config}"
cat files/config-slate.txt >> "${config}"
