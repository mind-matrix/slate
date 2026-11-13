#!/bin/bash
set -euo pipefail

if [ $# -ne 2 ]; then
	echo "usage: $0 <lvgl>/scripts/built_in_font <DejaVuSansMono.ttf>" >&2
	exit 2
fi

SOURCE_FONTS="$1"
MONO_FONT="$2"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ui/theme/fonts"
TEXT_RANGES="0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x2039-0x203A,0x2190-0x2193,0x2212"
MONO_RANGES="0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x2190-0x2193,0x2500-0x259F"
SYMBOLS="61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"

mkdir -p "${OUT_DIR}"
for size in 14 16 20 24 32 48; do
	lv_font_conv --no-compress --no-prefilter --bpp 4 --size "${size}" \
		--font "${SOURCE_FONTS}/Montserrat-Medium.ttf" -r "${TEXT_RANGES}" \
		--font "${SOURCE_FONTS}/FontAwesome5-Solid+Brands+Regular.woff" -r "${SYMBOLS}" \
		--format lvgl --force-fast-kern-format --lv-include lvgl.h \
		-o "${OUT_DIR}/slate_font_${size}.c"
done

lv_font_conv --no-compress --no-prefilter --no-kerning --bpp 4 --size 18 \
	--font "${MONO_FONT}" -r "${MONO_RANGES}" \
	--format lvgl --lv-include lvgl.h \
	-o "${OUT_DIR}/slate_mono_18.c"

lv_font_conv --no-compress --no-prefilter --bpp 4 --size 96 \
	--font "${SOURCE_FONTS}/Montserrat-Medium.ttf" -r "0x20,0x2B,0x2D-0x2E,0x30-0x3A" \
	--format lvgl --force-fast-kern-format --lv-include lvgl.h \
	-o "${OUT_DIR}/slate_digits_96.c"
