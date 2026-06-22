#!/usr/bin/env bash
# Stage openbox's config + theme into a VFS tree (mounted via the host --vm-tree, like the Xft fonts).
# This is data/config staging, NOT a source change: openbox loads its default rc.xml + the Clearlooks
# theme from the standard XDG paths, exactly as on a real LXDE install. Usage: prepare-openbox-fixtures.sh [outdir]
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
OUT="${1:-/tmp/vmopenbox}"
OBS="$EXP/third_party/openbox-threads"
[ -d "$OBS" ] || { echo "openbox source not built yet (run build-openbox.sh)"; exit 1; }
mkdir -p "$OUT/usr/share/themes/Clearlooks/openbox-3" "$OUT/etc/xdg/openbox"
cp "$OBS/themes/Clearlooks/openbox-3/themerc" "$OUT/usr/share/themes/Clearlooks/openbox-3/themerc"
cp "$OBS/data/rc.xml"   "$OUT/etc/xdg/openbox/rc.xml"
cp "$OBS/data/menu.xml" "$OUT/etc/xdg/openbox/menu.xml"
# a few alternate themes so a config referencing one still resolves
for t in Mikachu Onyx Natura; do
  mkdir -p "$OUT/usr/share/themes/$t/openbox-3"
  cp "$OBS/themes/$t/openbox-3/themerc" "$OUT/usr/share/themes/$t/openbox-3/" 2>/dev/null || true
done
echo "staged openbox fixtures in $OUT:"; find "$OUT" -type f
