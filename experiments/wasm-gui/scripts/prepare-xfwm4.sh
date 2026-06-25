#!/usr/bin/env bash
# Stage xfwm4's runtime DATA files (the program's own, not a theme package) into a VM tree:
#  - usr/share/xfwm4/defaults : the default-settings file. xfwm4 treats its ABSENCE as FATAL ("Missing
#    defaults file" -> exit 1), so this is required for xfwm4 to run at all.
#  - usr/share/themes/Default/xfwm4 : xfwm4's bundled fallback decoration theme (source dir themes/default).
# These come straight from the UNMODIFIED xfwm4 source tree (what `make install` would stage); the host
# installs the tree at the VM root via --vm-tree. Output: $1 (default /tmp/vmxfwm4).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
OUT="${1:-/tmp/vmxfwm4}"
SRC="$EXP/third_party/xfwm4"
[ -f "$SRC/defaults/defaults" ] || { echo "no xfwm4 source at $SRC (build-xfwm4.sh fetches it)"; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT/usr/share/xfwm4" "$OUT/usr/share/themes/Default"
cp -a "$SRC/defaults/defaults" "$OUT/usr/share/xfwm4/defaults"
# The bundled "default" theme installs under the theme NAME "Default".
cp -a "$SRC/themes/default" "$OUT/usr/share/themes/Default/xfwm4"
echo "staged xfwm4 defaults ($(wc -l < "$OUT/usr/share/xfwm4/defaults") lines) + Default fallback theme into $OUT"
