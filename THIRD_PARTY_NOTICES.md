# Third-party notices

Slate is released under the MIT License (see `LICENSE`). It includes or builds on the following third-party work.
Full licence texts are in `licenses/`.

## Fonts

The files `shell/ui/theme/fonts/slate_font_*.c` and `shell/ui/theme/fonts/slate_mono_18.c` are bitmap subsets generated
with lv_font_conv from the fonts below. They remain under the fonts' original licences.

| Font | Used in | Copyright | Licence |
|---|---|---|---|
| Montserrat (Medium) | `slate_font_*.c` | Copyright 2011 The Montserrat Project Authors (https://github.com/JulietaUla/Montserrat) | SIL Open Font License 1.1, `licenses/OFL-1.1.txt` |
| Font Awesome Free 5 (Solid, Brands, Regular), as bundled with LVGL | `slate_font_*.c` (symbol glyphs) | Font Awesome Free 5 by @fontawesome, https://fontawesome.com | Fonts: SIL Open Font License 1.1, `licenses/OFL-1.1.txt`; icons: CC BY 4.0, https://creativecommons.org/licenses/by/4.0/ |
| DejaVu Sans Mono | `slate_mono_18.c` | Copyright (c) 2003 by Bitstream, Inc. DejaVu changes are in the public domain. | Bitstream Vera Fonts licence, `licenses/DejaVu.txt` |

## Libraries

| Library | How it is used | Copyright | Licence |
|---|---|---|---|
| LVGL 9.2.2 | Fetched at build time and linked statically into `slate-shell` | Copyright (c) 2021 LVGL Kft | MIT, `licenses/LVGL-MIT.txt` |
| libvterm 0.3 | Linked dynamically into `slate-shell` (Debian package `libvterm0`) | Copyright 2008-2023 Paul Evans | MIT, `licenses/libvterm.txt` |
| libsystemd (sd-bus) | Linked dynamically into `slate-shell` | systemd contributors | LGPL-2.1-or-later |
| libdrm | Linked dynamically into `slate-shell` | libdrm contributors | MIT |

## Operating system images

Slate images are built from Raspberry Pi OS and Debian packages with pi-gen. Each package keeps its own licence; see
`/usr/share/doc/<package>/copyright` on a running system. Source for those packages is available from the Debian and
Raspberry Pi OS archives (https://deb.debian.org and https://archive.raspberrypi.com) for the versions in each image.
