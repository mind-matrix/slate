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
	file="$(basename "${@: -1}")"
	version="$(echo "$file" | sed -n 's/^slate_\(.*\)_arm64\.deb$/\1/p')"
	[ -n "$version" ] && sed -i "s/^SLATE_VERSION=.*/SLATE_VERSION=$version/" "$FAKE_RELEASE"
	app="$(echo "$file" | sed -n 's/^\(slate-app-[a-z0-9-]*\)_\(.*\)_arm64\.deb$/\1 \2/p')"
	[ -n "$app" ] && sed -i "s/^${app%% *} .*/$app/" "$FAKE_APPS"
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
cat > "$WORK/dpkg-deb" <<'EOF'
#!/bin/bash
[ "$1" = -f ] && head -c 200 "$2" | cut -d" " -f1
EOF
cat > "$WORK/dpkg-query" <<'EOF'
#!/bin/bash
package="${@: -1}"
version="$(sed -n "s/^$package //p" "$FAKE_APPS" 2>/dev/null)"
[ -n "$version" ] && printf %s "$version"
EOF
cat > "$WORK/systemctl" <<'EOF'
#!/bin/bash
echo "systemctl $*" >> "$FAKE_LOG"
EOF
chmod +x "$WORK/apt" "$WORK/dpkg" "$WORK/dpkg-deb" "$WORK/dpkg-query" "$WORK/systemctl"

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
		SLATE_UPDATE_DPKG_DEB="$WORK/dpkg-deb" SLATE_UPDATE_DPKG_QUERY="$WORK/dpkg-query" \
		SLATE_UPDATE_SYSTEMCTL="$WORK/systemctl" SLATE_APP_POLICY_DIR="$WORK/policy" \
		SLATE_APP_FEED_BASE="file://$WORK/apps" FAKE_APPS="$WORK/apps.installed" \
		bash "$UPDATER" "$@" >/dev/null 2>&1
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

minisign -G -W -f -p "$WORK/app.pub" -s "$WORK/app.sec" >/dev/null
APP_FEED="$WORK/apps/test/hello/releases/latest/download"
app_status_is() { grep -qx "$1" "$WORK/state/apps/hello/status"; }
app_version_is() { grep -qx "slate-app-hello $1" "$WORK/apps.installed"; }

reset_app() {
	reset_device 2026.11.20 "${1:-release}"
	[ -n "${2:-}" ] && printf 'SLATE_DEV_APPS=%s\n' "$2" >> "$WORK/image"
	rm -rf "$WORK/apps" "$WORK/policy"
	mkdir -p "$APP_FEED" "$WORK/policy"
	printf 'repo=test/hello\nkey=%s\npermissions=network\n' "$(tail -n 1 "$WORK/app.pub")" > "$WORK/policy/hello.conf"
	echo "slate-app-hello 0.1.0" > "$WORK/apps.installed"
	publish 2026.11.20
}

publish_app() {
	local version="$1" package="${2:-slate-app-hello}" key="${3:-$WORK/app.sec}"
	local deb="$APP_FEED/slate-app-hello_${version}_arm64.deb"
	echo "$package test package $version" > "$deb"
	jq -n --arg v "$version" --arg f "$(basename "$deb")" --arg u "file://$deb" \
		--arg h "$(sha256sum "$deb" | cut -d' ' -f1)" --argjson s "$(stat -c %s "$deb")" \
		'{version: $v, channel: "release", min_version: "0.1.0", remove: [],
		  package: {file: $f, url: $u, sha256: $h, size: $s}, notes: ["Hello note"]}' > "$APP_FEED/manifest.json"
	minisign -S -s "$key" -m "$APP_FEED/manifest.json" >/dev/null
}

reset_app
publish_app 0.2.0
run check
check "a check also checks installed apps" app_status_is state=available
check "an app update reports its own version" app_status_is version=0.2.0
check "Slate's own status is kept separate" status_is state=current

reset_app
publish_app 0.2.0
run check hello
run install hello
check "an app update installs the app's package" app_version_is 0.2.0
check "an installed app update is current without a reboot" app_status_is state=current
check "the app is restarted after its update" grep -q "systemctl try-restart slate-app@hello.service" "$WORK/log"
check "an app update keeps the package for recovery" test -f "$WORK/state/apps/hello/installed.deb"

reset_app
publish_app 0.2.0 slate-app-hello "$WORK/key.sec"
run check hello
check "an app manifest signed with another key is refused" app_status_is reason=unverified

reset_app
publish_app 0.2.0 slate
run check hello
run install hello
check "an app package that claims to be Slate is refused" app_status_is reason=unverified
check "a refused app package is not installed" app_version_is 0.1.0

reset_app release hello
publish_app 0.2.0
run check hello
check "a dev build of an app is not updated from GitHub" app_status_is state=current

reset_app
publish_app 0.2.0
run check ghost
check "an app without a Slate policy is not checked" test ! -e "$WORK/state/apps/ghost"

echo
if [ "$failures" -eq 0 ]; then
	echo "All updater checks passed"
else
	echo "$failures updater checks failed"
	exit 1
fi
