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
# xfwm4/ = the window-manager DECORATION theme (themerc + titlebar/border images), used by xfwm4 (XU2).
[ -d "$SRC/xfwm4" ] && cp -a "$SRC/xfwm4" "$DST/"
# ★ Transcode XPM-only decoration images to PNG. Our gdk-pixbuf is built PNG-only (no XPM loader), and
# xfwm4's built-in XPM reader fails on the wasi VFS ("Cannot read Pixmap header"); several Greybird
# border/corner elements ship ONLY as .xpm (no .png sibling), so without a PNG xfwm4 can't load them and
# abandons the whole frame. Provide a .png for every .xpm so xfwm4 loads the full frame via gdk-pixbuf.
# This is a data-asset transcode (constraint #5: xfwm4 itself is untouched).
if [ -d "$DST/xfwm4" ]; then
  python3 - "$DST/xfwm4" <<'PY'
import sys, os, glob
from PIL import Image
d = sys.argv[1]; n = 0
for xpm in glob.glob(os.path.join(d, "*.xpm")):
    png = xpm[:-4] + ".png"
    if os.path.exists(png):
        continue
    try:
        Image.open(xpm).convert("RGBA").save(png); n += 1
    except Exception as e:
        print(f"  xpm->png FAILED {os.path.basename(xpm)}: {e}", file=sys.stderr)
print(f"  transcoded {n} XPM-only xfwm4 images to PNG")
PY
fi
cp -a "$SRC/index.theme" "$DST/" 2>/dev/null || true
echo "staged Greybird theme: gtk-3.0 ($(stat -c%s "$DST/gtk-3.0/gtk.css") bytes css, $(ls "$DST/gtk-3.0/assets" 2>/dev/null | wc -l) assets) + xfwm4 ($(ls "$DST/xfwm4" 2>/dev/null | wc -l) deco files) into $OUT"
