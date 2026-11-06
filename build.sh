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
	prepare_pi_gen
	for stage in $custom_stages; do
		stage_paths="$stage_paths $SYNCED_DIR/stages/$stage"
	done
	mark_export_stage "${custom_stages##* }"
	write_pi_gen_config "$variant" "$stage_paths" "$image_date"

	(cd "$PI_GEN_DIR" && sudo ./build.sh)

	local image="$PI_GEN_DIR/deploy/$image_date-slate-$variant.img"
	cp "$image" "$OUTPUT_DIR/"
	echo "Image ready: $OUTPUT_DIR/$(basename "$image")"
}

main "$@"
