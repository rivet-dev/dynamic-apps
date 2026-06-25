#!/usr/bin/env bash
# XU6 fixture: stage the xfce4-notifyd "Default" notification theme so the popup CSS loads (else
# "theme 'Default' is not found" + the popup never paints). Installs to the vm-tree path notifyd
# searches: $(datadir)/themes/Default/xfce-notify-4.0/gtk.css. Constraint #5: a fixture, not a patch.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-/tmp/vmnotifyd}"; SRC="third_party/xfce4-notifyd/themes/gtk-3.20/Default/gtk.css"
[ -f "$SRC" ] || { echo "FATAL: $SRC not found"; exit 1; }
mkdir -p "$OUT/usr/share/themes/Default/xfce-notify-4.0"
cp "$SRC" "$OUT/usr/share/themes/Default/xfce-notify-4.0/gtk.css"
echo "staged Default notify theme -> $OUT/usr/share/themes/Default/xfce-notify-4.0/gtk.css"
