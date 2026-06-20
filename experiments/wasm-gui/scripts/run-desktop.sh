#!/usr/bin/env bash
# Launch the INTERACTIVE wasm desktop in a native window (winit + softbuffer; works on macOS and
# Linux). The X server + twm + xclock + a libX11 window + the XTEST input agent all run as wasm guests
# in one secure-exec VM; this host streams the live X framebuffer into the window and forwards your
# mouse/keyboard back through the XTEST agent.
#
# Needs a machine WITH A DISPLAY (this is the manual, on-screen demo — not headless).
# Build the host with the `window` feature first (done automatically below).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"

[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true

echo "building wasm-gui-host (window feature)..."
( cd "$REPO" && cargo build -p wasm-gui-host --features window ) || exit 1
HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar )

for f in "$EXP/Xvfb.wasm" "$EXP/twm.wasm" "$EXP/xclock.wasm" "$EXP/guest-xclient/xwin.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f — build the wasm artifacts first"; exit 1; }
done

# Input is forwarded by the host speaking X11+XTEST directly to the X server's host-backed socket
# (no in-VM agent needed). Move the mouse, click, and drag the window titlebars; type after clicking.
echo "launching interactive desktop window (close the window or press Esc to quit)..."
exec env -u DISPLAY=:0 "$HOST" --desktop \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/xclock.wasm -analog -update 1 -geometry 150x150+420+40" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts
