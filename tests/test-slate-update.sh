#!/bin/bash
set -uo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPDATER="$SOURCE_DIR/packaging/root/usr/lib/slate/slate-update"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0

check() {
	local description="$1"
	shift
	if "$@"; then
		echo "PASS  $description"
	else
		echo "FAIL  $description"
		failures=$((failures + 1))
	fi
}

status_is() { grep -qx "$1" "$WORK/state/status"; }
status_lacks() { ! grep -q "$1" "$WORK/state/status" 2>/dev/null; }
release_version_is() { grep -qx "SLATE_VERSION=$1" "$WORK/release"; }

minisign -G -W -f -p "$WORK/key.pub" -s "$WORK/key.sec" >/dev/null

cat > "$WORK/apt" <<'EOF'
#!/bin/bash
echo "$*" >> "$FAKE_LOG"
[ "${FAKE_APT_FAIL:-0}" = 1 ] && exit 1
if [ "$1" = install ]; then
	version="$(basename "${@: -1}" | sed -n 's/^slate_\(.*\)_arm64\.deb$/\1/p')"
	sed -i "s/^SLATE_VERSION=.*/SLATE_VERSION=$version/" "$FAKE_RELEASE"
fi
exit 0
EOF
cat > "$WORK/dpkg" <<'EOF'
#!/bin/bash
case "$1" in
	--compare-versions) exec dpkg "$@" ;;
	-s) echo " $FAKE_INSTALLED " | grep -q " $2 " ;;
	*) echo "dpkg $*" >> "$FAKE_LOG" ;;
esac
EOF
chmod +x "$WORK/apt" "$WORK/dpkg"

reset_device() {
	rm -rf "$WORK/state" "$WORK/feed"
	mkdir -p "$WORK/state" "$WORK/feed"
	printf 'SLATE_VERSION=%s\nSLATE_COMMIT=abc1234\nSLATE_CHANNEL=release\n' "${1:-2026.11.7}" > "$WORK/release"
	printf 'SLATE_BUILD_TYPE=%s\n' "${2:-release}" > "$WORK/image"
	: > "$WORK/log"
}

publish() {
	local version="$1" channel="${2:-release}" min="${3:-2026.11.7}" remove="${4:-[]}" hash="${5:-}"
	local deb="$WORK/feed/slate_${version}_arm64.deb"
	echo "slate test package $version" > "$deb"
	[ -n "$hash" ] || hash="$(sha256sum "$deb" | cut -d' ' -f1)"
	jq -n --arg v "$version" --arg c "$channel" --arg m "$min" --arg f "$(basename "$deb")" \
		--arg u "file://$deb" --arg h "$hash" --argjson s "$(stat -c %s "$deb")" --argjson r "$remove" \
		'{version: $v, channel: $c, min_version: $m, remove: $r,
		  package: {file: $f, url: $u, sha256: $h, size: $s}, notes: ["First note", "Second note"]}' \
		> "$WORK/feed/manifest.json"
	minisign -S -s "$WORK/key.sec" -m "$WORK/feed/manifest.json" >/dev/null
}

run() {
	SLATE_UPDATE_STATE="$WORK/state" SLATE_UPDATE_FEED="file://$WORK/feed" SLATE_UPDATE_KEY="$WORK/key.pub" \
		SLATE_RELEASE_FILE="$WORK/release" SLATE_IMAGE_FILE="$WORK/image" SLATE_UPDATE_APT="$WORK/apt" \
		SLATE_UPDATE_DPKG="$WORK/dpkg" SLATE_UPDATE_PROTOCOLS="=file" FAKE_LOG="$WORK/log" \
		FAKE_RELEASE="$WORK/release" FAKE_INSTALLED="${FAKE_INSTALLED:-}" \
		bash "$UPDATER" "$@" >/dev/null 2>&1
	return 0
}

reset_device 2026.11.7 dev
publish 2026.11.20
run check
check "dev builds do not check for updates" status_is state=disabled

reset_device
publish 2026.11.20
run check
check "a newer signed release is offered" status_is state=available
check "the offered version is recorded" status_is version=2026.11.20
check "release notes are saved" grep -qx "Second note" "$WORK/state/notes"
check "the check time is recorded" status_lacks checked=0
run install
check "the update installs and asks for a restart" status_is state=restart
check "the new version is installed" release_version_is 2026.11.20
check "the installed package is kept" test -f "$WORK/state/installed.deb"
check "the staging area is cleaned up" test ! -e "$WORK/state/staging"
check "the package cache is cleaned" grep -qx clean "$WORK/log"
run check
check "a check before restarting keeps the restart prompt" status_is state=restart
run recover
check "after the restart the device is up to date" status_is state=current

publish 2026.11.21
run check
run install
check "a later update keeps the previous package" test -f "$WORK/state/previous.deb"

reset_device 2026.11.20
publish 2026.11.20
run check
check "the same version is reported as up to date" status_is state=current

reset_device 2026.11.20
publish 2026.11.7
run check
check "an older release is never offered" status_is state=current

reset_device
publish 2026.11.20 beta
run check
check "a release on another channel is ignored" status_is state=current

reset_device
publish 2026.11.20 release 2026.11.10
run check
check "a release that cannot update this version asks for a new image" status_is reason=too_old

reset_device
publish 2026.11.20
sed -i 's/First note/Injected note/' "$WORK/feed/manifest.json"
run check
check "a manifest changed after signing is rejected" status_is reason=unverified
check "nothing is offered from a rejected manifest" test ! -e "$WORK/state/manifest.json"

reset_device
publish 2026.11.20 release 2026.11.7 '[]' "$(printf '0%.0s' {1..64})"
run check
run install
check "a package with the wrong hash is rejected" status_is reason=unverified
check "a rejected package is not installed" release_version_is 2026.11.7

reset_device
publish 2026.11.20 release 2026.11.7 '["slate"]'
run check
check "a manifest that removes Slate itself is rejected" status_is reason=unverified

reset_device
publish 2026.11.20 release 2026.11.7 '["oldtool", "absent"]'
run check
FAKE_INSTALLED=oldtool run install
check "packages a release no longer needs are removed" grep -qx "purge -y oldtool" "$WORK/log"
check "packages that are not installed are left alone" bash -c "! grep -q absent '$WORK/log'"

reset_device
publish 2026.11.20
run check
SLATE_UPDATE_FREE_KB=1024 run install
check "an update without enough space is refused" status_is reason=space

reset_device
publish 2026.11.20
run check
FAKE_APT_FAIL=1 run install
check "a failed install is reported" status_is reason=failed
check "a failed install keeps the installed version" release_version_is 2026.11.7

reset_device
rm -f "$WORK/feed/manifest.json"
run check
check "an unreachable release server is reported as offline" status_is reason=offline

reset_device
printf 'state=downloading\nversion=2026.11.20\n' > "$WORK/state/status"
run recover
check "an interrupted download is reported after a restart" status_is reason=interrupted
check "an interrupted update finishes pending package setup" grep -q "dpkg --configure -a" "$WORK/log"

reset_device 2026.11.20
printf 'state=installing\nversion=2026.11.20\n' > "$WORK/state/status"
run recover
check "an install that completed before a power cut counts as done" status_is state=current

echo
if [ "$failures" -eq 0 ]; then
	echo "All updater checks passed"
else
	echo "$failures updater checks failed"
	exit 1
fi
