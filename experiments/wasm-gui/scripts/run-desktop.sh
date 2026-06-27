#!/usr/bin/env bash
# Launch the INTERACTIVE wasm desktop in a native window (winit + softbuffer — works on macOS and Linux).
#
# The X server (Xvfb) + a window manager (twm) + apps (xclock, a libX11 window) all run as wasm guests
# inside ONE secure-exec VM. This host process streams the live X framebuffer into a native OS window and
# forwards your real mouse + keyboard back into the guest X server over X11/XTEST. Move the mouse, click,
# drag the window titlebars, and type after clicking a window. Press Esc or close the window to quit.
#
# Requires a machine WITH A DISPLAY:
#   - macOS: works out of the box (native Cocoa window; no DISPLAY needed).
#   - Linux: needs a running X11 (DISPLAY) or Wayland (WAYLAND_DISPLAY) session — your normal desktop.
#            winit's X11 backend needs these runtime libs (present on any normal desktop; on a MINIMAL box
#            install them): libx11, libxcb, libxkbcommon0, libxkbcommon-x11-0, libgl1.
#            To test headless on Linux with no desktop, use the containerized harness instead:
#              bash scripts/verify-window-headless.sh     # Docker + Xvfb, screenshots the window
#            or wrap this directly:  xvfb-run -s "-screen 0 1280x1024x24" bash scripts/run-desktop.sh
#
# The wasm guests are platform-independent; only this thin host differs per-OS, and winit abstracts that.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"

# Guest screen geometry. The winit window auto-sizes to match this (parsed from the -screen arg below).
W="${SX_GUI_WIDTH:-800}"
H="${SX_GUI_HEIGHT:-600}"

FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"
[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true

echo "building wasm-gui-host (window feature) + sidecar..."
( cd "$REPO" && cargo build -p wasm-gui-host --features window ) || exit 1
HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar ) || exit 1

for f in "$EXP/Xvfb.wasm" "$EXP/twm.wasm" "$EXP/xclock.wasm" "$EXP/guest-xclient/xwin.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f — build the wasm artifacts first"; exit 1; }
done

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(uname)" != "Darwin" ]; then
  echo "WARNING: no DISPLAY or WAYLAND_DISPLAY set — winit has no Linux display to open a window on."
  echo "         Run inside your desktop session, or headless via: xvfb-run -s \"-screen 0 ${W}x${H}x24\" $0"
fi

echo "launching interactive wasm desktop (${W}x${H}) — Esc or close the window to quit..."
# Do NOT override DISPLAY here: winit needs the HOST's real display (macOS native / your Linux X or
# Wayland). The ':0' inside the guest args below is the GUEST X server's display, which is separate.
exec "$HOST" --desktop \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/xclock.wasm -analog -update 1 -geometry 150x150+600+40" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --sidecar "$SIDECAR" \
  -- :0 -screen 0 "${W}x${H}x24" -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts
