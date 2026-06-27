#!/usr/bin/env bash
# Launch the INTERACTIVE wasm desktop in a native window (winit + softbuffer — works on macOS and Linux).
#
# The X server (Xvfb) + a window manager (twm) + apps (xclock, a libX11 window) all run as wasm guests
# inside ONE secure-exec VM. This host process streams the live X framebuffer into a native OS window and
# forwards your real mouse + keyboard back into the guest X server over X11/XTEST. Move the mouse, click,
# drag the window titlebars, and type after clicking a window. Press Esc or close the window to quit.
#
# SELF-CONTAINED: the prebuilt wasm guests + fonts + locale ship in experiments/wasm-gui/demo-assets/, so a
# fresh checkout runs on macOS or Linux WITHOUT the wasm toolchain — you only need Rust (to build this thin
# host) and a display. (A fresh checkout also needs `pnpm install` once so the V8 bridge assets generate;
# see the repo CLAUDE.md.)
#
# Requires a machine WITH A DISPLAY:
#   - macOS: works out of the box (native Cocoa window; no DISPLAY needed).
#   - Linux: needs a running X11 (DISPLAY) or Wayland (WAYLAND_DISPLAY) session — your normal desktop.
#            winit's X11 backend needs these runtime libs (present on any normal desktop; on a MINIMAL box
#            install: libx11 libxcb libxkbcommon0 libxkbcommon-x11-0 libgl1).
#            To run headless on Linux, use the container harness: bash scripts/verify-window-headless.sh
#            or wrap this: xvfb-run -s "-screen 0 1280x1024x24" bash scripts/run-desktop.sh
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
ASSETS="$EXP/demo-assets"

# Guest screen geometry. The winit window auto-sizes to match this (parsed from the -screen arg below).
W="${SX_GUI_WIDTH:-800}"
H="${SX_GUI_HEIGHT:-600}"

# Prefer the committed self-contained bundle; fall back to a locally-staged tree if you built your own.
FONTS="${VMFONTS:-$ASSETS/fonts}";  [ -d "$FONTS" ]  || FONTS=/tmp/vmfonts
LOCALE="${VMLOCALE:-$ASSETS/locale}"; [ -d "$LOCALE" ] || LOCALE=/tmp/vmlocale
XVFB="$ASSETS/Xvfb.wasm"; TWM="$ASSETS/twm.wasm"; XCLOCK="$ASSETS/xclock.wasm"; XWIN="$ASSETS/xwin.wasm"
# Fall back to the top-level build outputs if the bundle is absent (in-repo dev builds).
[ -f "$XVFB" ]   || XVFB="$EXP/Xvfb.wasm"
[ -f "$TWM" ]    || TWM="$EXP/twm.wasm"
[ -f "$XCLOCK" ] || XCLOCK="$EXP/xclock.wasm"
[ -f "$XWIN" ]   || XWIN="$EXP/guest-xclient/xwin.wasm"

echo "building wasm-gui-host (window feature) + sidecar..."
( cd "$REPO" && cargo build -p wasm-gui-host --features window ) || {
  echo "cargo build failed — a fresh checkout needs 'pnpm install' first (V8 bridge assets); see CLAUDE.md"; exit 1; }
HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar ) || exit 1

for f in "$XVFB" "$TWM" "$XCLOCK" "$XWIN"; do
  [ -f "$f" ] || { echo "MISSING guest wasm: $f (expected the demo-assets bundle)"; exit 1; }
done

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(uname)" != "Darwin" ]; then
  echo "WARNING: no DISPLAY or WAYLAND_DISPLAY set — winit has no Linux display to open a window on."
  echo "         Run inside your desktop session, or headless via: xvfb-run -s \"-screen 0 ${W}x${H}x24\" $0"
fi

echo "launching interactive wasm desktop (${W}x${H}) — Esc or close the window to quit..."
# Do NOT override DISPLAY here: winit needs the HOST's real display (macOS native / your Linux X or
# Wayland). The ':0' inside the guest args below is the GUEST X server's display, which is separate.
exec "$HOST" --desktop \
  --server "$XVFB" \
  --client "$TWM" \
  --client "$XCLOCK -analog -update 1 -geometry 150x150+600+40" \
  --client "$XWIN" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --sidecar "$SIDECAR" \
  -- :0 -screen 0 "${W}x${H}x24" -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts
