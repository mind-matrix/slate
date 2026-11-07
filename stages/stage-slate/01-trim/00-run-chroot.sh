#!/bin/bash -e

apt-get purge -y cloud-init rpi-cloud-init-mods udisks2 cron man-db avahi-daemon
apt-get autoremove --purge -y
systemctl mask apt-daily.timer apt-daily-upgrade.timer e2scrub_all.timer
