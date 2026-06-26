#!/usr/bin/env bash
# Render ONE wasm GTK/X app SOLO with the FULL fixture set. The FONT fixtures are MANDATORY: without the
# fontconfig config + staged fonts (/tmp/vmxft + /tmp/vmfonts), fontconfig logs "Cannot load default config
# file" / "No writable cache directories", loads NO fonts, and every glyph falls back to .notdef -> text
# renders as TOFU SQUARES, not real letters (a high non-black % then MASKS the failure). Any app screenshot
# MUST be produced through this helper (or an equivalent that stages fonts) -- never with a bare --fonts-dir
# pointing at an empty dir. Usage: render-app.sh <app.wasm> [W H <png>]
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
APP="$1"; W="${2:-800}"; H="${3:-600}"
PNG="${4:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/render-$(basename "$APP" .wasm).png}"
mkdir -p "$(dirname "$PNG")"
# MANDATORY fixtures.
find /tmp/vmfonts -name '*.ttf' 2>/dev/null | grep -q . || bash scripts/prepare-fonts.sh >/dev/null 2>&1
bash scripts/prepare-xftfonts.sh /tmp/vmxft >/dev/null 2>&1
[ -d /tmp/vmlocale ] || bash scripts/prepare-locale.sh /tmp/vmlocale >/dev/null 2>&1
bash scripts/stage-gschemas.sh /tmp/vmschemas >/dev/null 2>&1
[ -d /tmp/vmxu5sess ] || bash scripts/prepare-dbus-fixtures.sh /tmp/vmxu5sess >/dev/null 2>&1
FB="$(mktemp /tmp/render-fb.XXXXXX.bin)"
timeout 220 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-120}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" --client "$APP" \
  --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
  --vm-tree /tmp/vmxu5sess --vm-tree /tmp/vmicons --vm-tree /tmp/vmxft --vm-tree /tmp/vmschemas \
  --fb-out "$FB" --sidecar "$SIDECAR" -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "${RENDER_LOG:-/tmp/render-app.log}" 2>&1 || true
FCERR=$(grep -aic 'Fontconfig error' "${RENDER_LOG:-/tmp/render-app.log}")
echo "fontconfig errors: $FCERR  (MUST be 0 -- nonzero => tofu squares, not real text)"
[ -s "$FB" ] && python3 scripts/fb2png.py "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
echo "PNG: $PNG"
[ "$FCERR" = "0" ] || echo "WARNING: fontconfig errored -> the screenshot has TOFU, not real glyphs"
