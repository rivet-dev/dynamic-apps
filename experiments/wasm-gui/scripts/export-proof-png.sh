#!/usr/bin/env bash
# Export fresh PNG proof that the wasm desktop works: runs the real host (wasm guests in the
# secure-exec V8 sidecar) for the JWM desktop and the interactive input/drag scenarios, captures the
# live framebuffer from the host-backed shadow fs, and converts it to PNG in ~/tmp/gui-progress/.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
OUT="$HOME/tmp/gui-progress"; mkdir -p "$OUT"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; JWMCFG="${VMJWM:-/tmp/vmjwm}"
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1
[ -d "$JWMCFG" ] || bash "$EXP/scripts/prepare-jwm.sh" "$JWMCFG" >/dev/null 2>&1

# --- Proof 1: JWM desktop shell (panel/taskbar/live clock) managing a real libX11 window ---
FB1="$(mktemp /tmp/proof-jwm.XXXXXX.bin)"
echo "== capturing JWM desktop =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 26 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/jwm.wasm" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --vm-tree "$XFT" --vm-tree "$JWMCFG" \
  --fb-out "$FB1" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  > /tmp/proof-jwm.log 2>&1 || true
python3 "$EXP/scripts/fb2png.py" "$FB1" "$OUT/proof-m7-jwm-desktop.png" 640 480 && rm -f "$FB1"

# --- Proof 2: interactive drag (host XTEST drags a twm-decorated window to a new position) ---
FB2="$(mktemp /tmp/proof-drag.XXXXXX.bin)"
echo "== capturing interactive drag =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 26 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  --inject "h=motion 160 71" --inject "h=buttondn 1" \
  --inject "h=motion 250 180" --inject "h=motion 360 280" --inject "h=motion 430 330" --inject "h=buttonup 1" \
  --fb-out "$FB2" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  > /tmp/proof-drag.log 2>&1 || true
python3 "$EXP/scripts/fb2png.py" "$FB2" "$OUT/proof-m6-interactive-drag.png" 640 480 && rm -f "$FB2"

echo "== proof PNGs in $OUT =="
ls -la "$OUT"/proof-*.png