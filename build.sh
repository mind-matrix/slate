#!/bin/bash
set -euo pipefail

PI_GEN_REPO=https://github.com/RPi-Distro/pi-gen.git
PI_GEN_COMMIT=9bf6c9ae27c71a967a78694885e3ead61ca53c28

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SLATE_BUILD_ROOT:-$HOME/slate-build}"
SYNCED_DIR="$BUILD_ROOT/src"
PI_GEN_DIR="$BUILD_ROOT/pi-gen"
OUTPUT_DIR="${SLATE_OUTPUT_DIR:-/mnt/c/Users/sagni/Downloads}"

usage() {
	echo "usage: $0 dev|release" >&2
	exit 1
}

custom_stages_for() {
	case "$1" in
		dev) echo "stage-slate stage-slate-dev" ;;
		release) echo "stage-slate" ;;
		*) usage ;;
	esac
}

sync_sources() {
	mkdir -p "$SYNCED_DIR"
	rsync -a --delete --chmod=D755,F644 --exclude .git "$SOURCE_DIR/" "$SYNCED_DIR/"
	find "$SYNCED_DIR/stages" -name '*.sh' -exec chmod 755 {} +
}

prepare_pi_gen() {
	if [ ! -d "$PI_GEN_DIR/.git" ]; then
		git clone --quiet "$PI_GEN_REPO" "$PI_GEN_DIR"
	fi
	if ! git -C "$PI_GEN_DIR" cat-file -e "$PI_GEN_COMMIT^{commit}" 2>/dev/null; then
		git -C "$PI_GEN_DIR" fetch --quiet origin
	fi
	git -C "$PI_GEN_DIR" checkout --quiet --detach "$PI_GEN_COMMIT"
	touch "$PI_GEN_DIR"/stage{3,4,5}/SKIP "$PI_GEN_DIR"/stage{2,4,5}/SKIP_IMAGES
}

build_package() {
	"$SOURCE_DIR/shell/build.sh" pi
	local package
	package="$("$SOURCE_DIR/packaging/build-deb.sh" "$BUILD_ROOT/shell/pi/slate-shell" "$BUILD_ROOT/deb")"
	install -D -m 644 "$package" "$SYNCED_DIR/stages/stage-slate/05-shell/files/slate.deb"
}

policy_value() {
	sed -n "s/^$1=\(.*\)$/\1/p" "$2" | head -n 1
}

local_source_for() {
	local name="SLATE_APP_SOURCE_${1//-/_}"
	[ -f "$SYNCED_DIR/config/local.conf" ] || return 0
	(set +u; . "$SYNCED_DIR/config/local.conf"; echo "${!name:-}")
}

local_source_ids() {
	[ -f "$SYNCED_DIR/config/local.conf" ] || return 0
	sed -n 's/^export SLATE_APP_SOURCE_\([a-z0-9_]*\)=.*/\1/p' "$SYNCED_DIR/config/local.conf" | tr _ -
}

fetch_release() {
	local id="$1" policy="$2" out="$3" work feed file
	work="$(mktemp -d)"
	feed="https://github.com/$(policy_value repo "$policy")/releases/latest/download"
	curl -fsSL --proto =https -o "$work/manifest.json" "$feed/manifest.json"
	curl -fsSL --proto =https -o "$work/manifest.json.minisig" "$feed/manifest.json.minisig"
	minisign -Vq -P "$(policy_value key "$policy")" -m "$work/manifest.json" -x "$work/manifest.json.minisig"
	file="$(jq -er .package.file "$work/manifest.json")"
	[[ "$file" == "slate-app-${id}_"*"_arm64.deb" ]]
	curl -fsSL --proto =https -o "$work/$file" "$(jq -er .package.url "$work/manifest.json")"
	echo "$(jq -er .package.sha256 "$work/manifest.json")  $work/$file" | sha256sum -c --quiet -
	[ "$(dpkg-deb -f "$work/$file" Package)" = "slate-app-$id" ]
	install -m 644 "$work/$file" "$out/"
	rm -rf "$work"
}

build_local_app() {
	local id="$1" source="$2" out="$3" deb
	deb="$("$source/packaging/build-deb.sh" "$BUILD_ROOT/apps/$id" | tail -n 1)"
	[ "$(dpkg-deb -f "$deb" Package)" = "slate-app-$id" ]
	install -m 644 "$deb" "$out/"
	echo "$id" >> "$out/dev-apps"
}

bundle_apps() {
	local variant="$1" out="$SYNCED_DIR/stages/stage-slate/06-apps/files" policy id source
	rm -f "$out"/*.deb "$out/dev-apps"
	mkdir -p "$out"
	for id in $(local_source_ids); do
		[ "$variant" = dev ] || break
		source="$(local_source_for "$id")"
		[ -n "$source" ] && build_local_app "$id" "$source" "$out"
	done
	for policy in "$SOURCE_DIR"/packaging/root/usr/share/slate/app-policy/*.conf; do
		[ -f "$policy" ] || continue
		id="$(basename "$policy" .conf)"
		grep -qx "$id" "$out/dev-apps" 2>/dev/null && continue
		fetch_release "$id" "$policy" "$out"
	done
}

copy_to_windows() {
	local source target
	source="$(wslpath -w "$1")"
	target="$(wslpath -w "$2")"
	powershell.exe -NoProfile -Command "Copy-Item -LiteralPath '$source' -Destination '$target' -Force"
}

mark_export_stage() {
	echo 'IMG_SUFFIX=""' > "$SYNCED_DIR/stages/$1/EXPORT_IMAGE"
}

write_pi_gen_config() {
	local variant="$1" stage_paths="$2" image_date="$3"
	{
		cat "$SYNCED_DIR/config/common.conf" "$SYNCED_DIR/config/$variant.conf"
		if [ -f "$SYNCED_DIR/config/local.conf" ]; then
			cat "$SYNCED_DIR/config/local.conf"
		fi
		echo "export SLATE_BUILD_TYPE=$variant"
		echo "export IMG_NAME=slate-$variant"
		echo "export IMG_DATE=$image_date"
		echo "export STAGE_LIST=\"stage0 stage1 stage2 $stage_paths\""
	} > "$PI_GEN_DIR/config"
}

main() {
	local variant="${1:-}"
	local custom_stages stage_paths="" stage image_date
	custom_stages="$(custom_stages_for "$variant")"
	image_date="$(date +%Y-%m-%d)"

	sync_sources
	build_package
	bundle_apps "$variant"
	prepare_pi_gen
	for stage in $custom_stages; do
		stage_paths="$stage_paths $SYNCED_DIR/stages/$stage"
	done
	mark_export_stage "${custom_stages##* }"
	write_pi_gen_config "$variant" "$stage_paths" "$image_date"

	(cd "$PI_GEN_DIR" && sudo ./build.sh)

	local image="$PI_GEN_DIR/deploy/$image_date-slate-$variant.img"
	copy_to_windows "$image" "$OUTPUT_DIR/"
	echo "Image ready: $OUTPUT_DIR/$(basename "$image")"
}

main "$@"
