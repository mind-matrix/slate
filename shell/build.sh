#!/bin/bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SLATE_BUILD_ROOT:-${HOME}/slate-build}/shell"
SYNCED_DIR="${WORK_DIR}/src"
BUILD_IMAGE="slate-shell-build"
SCREENS=(desktop settings about diagnostics)

usage() {
	echo "usage: $0 sim|run|snapshot|pi" >&2
	exit 2
}

sync_sources() {
	mkdir -p "${SYNCED_DIR}"
	rsync -a --delete --exclude build "${SOURCE_DIR}/" "${SYNCED_DIR}/"
}

build_sim() {
	cmake -S "${SYNCED_DIR}" -B "${WORK_DIR}/sim" -DSLATE_BACKEND=sdl > /dev/null
	cmake --build "${WORK_DIR}/sim" -j2
	ctest --test-dir "${WORK_DIR}/sim" --output-on-failure
}

take_snapshots() {
	local out="${WORK_DIR}/snapshots"
	mkdir -p "${out}"
	for screen in "${SCREENS[@]}"; do
		"${WORK_DIR}/sim/slate-snapshot" "${out}/${screen}.rgb565" "${screen}"
		python3 "${SYNCED_DIR}/tools/rgb565_to_png.py" "${out}/${screen}.rgb565" "${out}/${screen}.png"
		echo "${out}/${screen}.png"
	done
}

build_pi() {
	docker build --platform linux/arm64 -q -t "${BUILD_IMAGE}" "${SYNCED_DIR}" > /dev/null
	docker run --rm --platform linux/arm64 -u "$(id -u):$(id -g)" -v "${WORK_DIR}:/work" -w /work "${BUILD_IMAGE}" \
		sh -c "cmake -S src -B pi -DSLATE_BACKEND=drm > /dev/null && cmake --build pi -j2 && ctest --test-dir pi --output-on-failure"
	echo "${WORK_DIR}/pi/slate-shell"
}

main() {
	[ $# -eq 1 ] || usage
	sync_sources
	case "$1" in
		sim) build_sim ;;
		run) build_sim && "${WORK_DIR}/sim/slate-shell" ;;
		snapshot) build_sim && take_snapshots ;;
		pi) build_pi ;;
		*) usage ;;
	esac
}

main "$@"
