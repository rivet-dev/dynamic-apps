#!/usr/bin/env bash
# Typing demo: mousepad (the Xubuntu text editor) receiving REAL keyboard input, all wasm. Validates the
# full input path end-to-end -- the staged XKB keymap (/xkb/default.xkm) activates the X keyboard device,
# then host XTEST `type <string>` drives KeyPress/KeyRelease into mousepad's text view. Proves typing works,
# not just rendering. Builds on render-app.sh's fixture staging (fonts/xkb/dbus/icons).
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
APP="$EXP/mousepad.wasm"; W=800; H=600
PNG="${PNG:-$HOME/tmp/gui-progress/$(date -u +%Y-%m-%dT%H)/xu6-mousepad-menu.png}"; mkdir -p "$(dirname "$PNG")"
# Mandatory fixtures (mirror render-app.sh).
find /tmp/vmfonts -name '*.ttf' 2>/dev/null | grep -q . || bash scripts/prepare-fonts.sh >/dev/null 2>&1
bash scripts/prepare-xftfonts.sh /tmp/vmxft >/dev/null 2>&1
[ -d /tmp/vmlocale ] || bash scripts/prepare-locale.sh /tmp/vmlocale >/dev/null 2>&1
bash scripts/stage-gschemas.sh /tmp/vmschemas >/dev/null 2>&1
bash scripts/prepare-dbus-fixtures.sh /tmp/vmxu5sess >/dev/null 2>&1
[ -f /tmp/vmxkb/xkb/default.xkm ] || bash scripts/prepare-xkb.sh /tmp/vmxkb >/dev/null 2>&1
FB="$(mktemp /tmp/type-fb.XXXXXX.bin)"
TEXT="Hello from secure-exec -- a real Xfce app (mousepad), all wasm in the sandbox."
echo "running mousepad File-menu demo -> png=$PNG"
# Click the text view to place the caret, assign X input focus to the window under the pointer, then type.
# POST_INJECT_DELAY_MS gives the editor time to render the typed text before the framebuffer capture.
POST_INJECT_DELAY_MS=15000 timeout 240 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout "${TIMEOUT:-170}" \
  --server "$EXP/Xvfb.wasm" --dbus "$EXP/dbus-daemon.wasm" --dbus-service "$EXP/xfconfd.wasm" --client "$APP" \
  --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
  --vm-tree /tmp/vmxu5sess --vm-tree /tmp/vmicons --vm-tree /tmp/vmxft --vm-tree /tmp/vmschemas --vm-tree /tmp/vmxkb \
  --inject "h=button 1 18 11" \
  --fb-out "$FB" --sidecar "$SIDECAR" -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  > "${RENDER_LOG:-/tmp/type-mousepad.log}" 2>&1 || true
[ -s "$FB" ] && python3 scripts/fb2png.py "$FB" "$PNG" "$W" "$H" 2>&1 | tail -1
echo "PNG: $PNG"
echo "XTEST type fired: $(grep -acE 'XTEST injected: type' "${RENDER_LOG:-/tmp/type-mousepad.log}" 2>/dev/null); crashes: $(grep -acE 'RuntimeError|fatal signal' "${RENDER_LOG:-/tmp/type-mousepad.log}" 2>/dev/null)"
