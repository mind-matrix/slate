#!/bin/bash -e

if [ -s files/dev-apps ]; then
	printf 'SLATE_DEV_APPS="%s"\n' "$(tr '\n' ' ' < files/dev-apps | sed 's/ $//')" >> "${ROOTFS_DIR}/etc/slate-release"
fi
for deb in files/*.deb; do
	[ -e "$deb" ] || continue
	install -D -m 644 "$deb" "${ROOTFS_DIR}/tmp/slate-apps/$(basename "$deb")"
done
if [ -d "${ROOTFS_DIR}/tmp/slate-apps" ]; then
	on_chroot <<CHROOT
apt-get install -y --no-install-recommends /tmp/slate-apps/*.deb
rm -rf /tmp/slate-apps
CHROOT
fi
