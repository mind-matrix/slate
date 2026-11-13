#!/bin/bash
set -uo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HELPER="$SOURCE_DIR/packaging/root/usr/lib/slate/slate-apps"
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

mkdir -p "$WORK/bin"
cat > "$WORK/bin/systemctl" <<'EOF'
#!/bin/bash
echo "systemctl $*" >> "$FAKE_LOG"
EOF
cat > "$WORK/bin/useradd" <<'EOF'
#!/bin/bash
echo "useradd ${@: -1}" >> "$FAKE_LOG"
echo "${@: -1}" >> "$FAKE_USERS"
EOF
cat > "$WORK/bin/groupadd" <<'EOF'
#!/bin/bash
echo "groupadd ${@: -1}" >> "$FAKE_LOG"
echo "${@: -1}" >> "$FAKE_USERS"
EOF
cat > "$WORK/bin/getent" <<'EOF'
#!/bin/bash
grep -qx "$2" "$FAKE_USERS" 2>/dev/null
EOF
chmod +x "$WORK/bin/"*

ROOT="$WORK/root"
DROPIN="$ROOT/etc/systemd/system/slate-app@ava.service.d/50-permissions.conf"
RULE="$ROOT/etc/polkit-1/rules.d/60-slate-app-ava.rules"
GRANTED="$ROOT/run/slate/apps/ava.permissions"

reset_root() {
	rm -rf "$ROOT"
	mkdir -p "$ROOT/usr/share/slate/apps" "$ROOT/usr/share/slate/app-policy" "$ROOT/etc"
	cp -r "$SOURCE_DIR/packaging/root/usr/share/slate/permissions" "$ROOT/usr/share/slate/"
	: > "$WORK/log"
	: > "$WORK/users"
	printf 'SLATE_BUILD_TYPE=release\n' > "$ROOT/etc/slate-release"
}

app() {
	printf 'name=%s\npermissions=%s\n' "$2" "$3" > "$ROOT/usr/share/slate/apps/$1.app"
}

policy() {
	printf 'repo=example/%s\nkey=RWQ\npermissions=%s\n' "$1" "$2" > "$ROOT/usr/share/slate/app-policy/$1.conf"
}

run() {
	SLATE_APPS_ROOT="$ROOT" SLATE_APPS_SYSTEMCTL="$WORK/bin/systemctl" SLATE_APPS_USERADD="$WORK/bin/useradd" \
		SLATE_APPS_GROUPADD="$WORK/bin/groupadd" SLATE_APPS_GETENT="$WORK/bin/getent" \
		FAKE_LOG="$WORK/log" FAKE_USERS="$WORK/users" bash "$HELPER" "$@" >/dev/null 2>&1
}

logged() { grep -qx "$1" "$WORK/log"; }
granted_is() { [ "$(cat "$GRANTED")" = "$1" ]; }

reset_root
app ava Ava "network wifi gui terminal"
policy ava "network wifi audio"
run sync
check "an app gets only permissions it requests and is granted" granted_is "network wifi"
check "network adds internet sockets to the app's sandbox" grep -qx "RestrictAddressFamilies=AF_INET AF_INET6 AF_NETLINK" "$DROPIN"
check "a permission that is not granted adds nothing" bash -c "! grep -q slate-display '$DROPIN'"
check "wifi becomes a polkit rule for the app's own user" grep -q '"org.freedesktop.NetworkManager."' "$RULE"
check "the polkit rule only applies to the app's user" grep -q 'subject.user !== "app-ava"' "$RULE"
check "the app gets its own system user" logged "useradd app-ava"
check "the app's service is enabled" logged "systemctl enable slate-app@ava.service"

: > "$WORK/log"
run sync
check "an existing app user is not created again" bash -c "! grep -q useradd '$WORK/log'"

reset_root
app hello Hello "gui overlay network"
policy hello "gui overlay network"
run sync
check "display permissions add the display group once" grep -qx "SupplementaryGroups=slate-display" "$ROOT/etc/systemd/system/slate-app@hello.service.d/50-permissions.conf"
check "a missing group is created" logged "groupadd slate-display"
check "display unit lines shared by gui and overlay are written once" \
	test "$(grep -c '^RuntimeDirectory=slate-apps/%i$' "$ROOT/etc/systemd/system/slate-app@hello.service.d/50-permissions.conf")" = 1
check "display permissions point the app at the shell's Wayland socket" \
	grep -q "WAYLAND_DISPLAY=/run/slate-display/wayland-0" "$ROOT/etc/systemd/system/slate-app@hello.service.d/50-permissions.conf"
check "an app without polkit permissions gets no polkit rule" test ! -e "$ROOT/etc/polkit-1/rules.d/60-slate-app-hello.rules"

reset_root
app ghost Ghost "network"
run sync
check "an app Slate has no policy for is not enabled" bash -c "! grep -q 'slate-app@ghost' '$WORK/log'"
check "an app Slate has no policy for gets no sandbox changes" test ! -e "$ROOT/etc/systemd/system/slate-app@ghost.service.d"

reset_root
printf 'SLATE_BUILD_TYPE=dev\nSLATE_DEV_APPS=ava\n' > "$ROOT/etc/slate-release"
app ava Ava "network secrets made-up"
run sync
check "a local dev build gets the catalog permissions it requests" granted_is "network secrets"

reset_root
app ava Ava "network wifi"
policy ava "network wifi"
run sync
rm "$ROOT/usr/share/slate/apps/ava.app"
run sync
check "a removed app's sandbox changes are deleted" test ! -e "$ROOT/etc/systemd/system/slate-app@ava.service.d"
check "a removed app's polkit rule is deleted" test ! -e "$RULE"
check "a removed app's service is disabled" logged "systemctl disable slate-app@ava.service"

reset_root
app ../evil Evil "network"
run sync
check "an app id that isn't a plain name is ignored" test -z "$(ls "$ROOT/etc/systemd/system" 2>/dev/null)"

echo
if [ "$failures" -eq 0 ]; then
	echo "All app permission checks passed"
else
	echo "$failures app permission checks failed"
	exit 1
fi
