#!/usr/bin/env bash
# Stage the authentic Xubuntu 24.04 Greybird gtk-3.0 theme into a VM tree for GTK apps. The theme is
# the real prebuilt asset from the Ubuntu noble `greybird-gtk-theme` package (vendored under
# third_party/greybird-theme/), NOT a hand-written substitute -- it ships the compiled gtk-3.0/gtk.css
# (sass-built upstream) + its assets/ images + index.theme. GTK resolves the theme by NAME from the X
# XSETTINGS Net/ThemeName that xfsettingsd publishes, then loads usr/share/themes/<name>/gtk-3.0/gtk.css.
# The host installs this tree at the VM root via --vm-tree. Output: $1 (default /tmp/vmthemes).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
OUT="${1:-/tmp/vmthemes}"
SRC="$EXP/third_party/greybird-theme/Greybird"
[ -d "$SRC/gtk-3.0" ] || { echo "no vendored Greybird at $SRC (run the deb-extract staging)"; exit 1; }
DST="$OUT/usr/share/themes/Greybird"
rm -rf "$DST"
mkdir -p "$DST"
cp -a "$SRC/gtk-3.0" "$DST/"
cp -a "$SRC/index.theme" "$DST/" 2>/dev/null || true
echo "staged Greybird gtk-3.0 theme ($(stat -c%s "$DST/gtk-3.0/gtk.css") bytes css, $(ls "$DST/gtk-3.0/assets" 2>/dev/null | wc -l) assets) into $OUT"
