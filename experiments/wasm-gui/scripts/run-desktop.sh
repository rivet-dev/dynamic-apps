#!/usr/bin/env bash
# Launch the INTERACTIVE wasm desktop in a native window (winit + softbuffer — works on macOS and Linux).
#
# The X server (Xvfb) + a window manager (twm) + apps (pcmanfm, Mousepad when built) all run as wasm guests
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
ALLOW_FALLBACK="${SX_GUI_ALLOW_FALLBACK:-0}"

# Guest screen geometry. The native window displays this framebuffer scaled up by SX_GUI_SCALE.
W="${SX_GUI_WIDTH:-1280}"
H="${SX_GUI_HEIGHT:-800}"
SCALE="${SX_GUI_SCALE:-2}"

# Prefer the committed self-contained bundle; fall back to a locally-staged tree if you built your own.
FONTS="${VMFONTS:-$ASSETS/fonts}";  [ -d "$FONTS" ]  || FONTS=/tmp/vmfonts
LOCALE="${VMLOCALE:-$ASSETS/locale}"; [ -d "$LOCALE" ] || LOCALE=/tmp/vmlocale
XVFB="$ASSETS/Xvfb.wasm"; TWM="$ASSETS/twm.wasm"; XCLOCK="$ASSETS/xclock.wasm"; XWIN="$ASSETS/xwin.wasm"
DBUS="$ASSETS/dbus-daemon.wasm"; XFCONFD="$ASSETS/xfconfd.wasm"
# Fall back to the top-level build outputs if the bundle is absent (in-repo dev builds).
[ -f "$XVFB" ]   || XVFB="$EXP/Xvfb.wasm"
[ -f "$TWM" ]    || TWM="$EXP/twm.wasm"
[ -f "$XCLOCK" ] || XCLOCK="$EXP/xclock.wasm"
[ -f "$XWIN" ]   || XWIN="$EXP/guest-xclient/xwin.wasm"
[ -f "$DBUS" ]   || DBUS="$EXP/dbus-daemon.wasm"
[ -f "$XFCONFD" ] || XFCONFD="$EXP/xfconfd.wasm"

is_wasm_binary() {
  python3 - "$1" <<'PY'
import sys
try:
    data = open(sys.argv[1], 'rb').read(8)
except OSError:
    sys.exit(1)
sys.exit(0 if data == b'\0asm\1\0\0\0' else 1)
PY
}

wasm_imports_name() {
  python3 - "$1" "$2" <<'PY'
import sys
path, want = sys.argv[1], sys.argv[2]
data = open(path, 'rb').read()
if data[:8] != b'\0asm\1\0\0\0':
    sys.exit(1)
i = 8
def uleb():
    global i
    result = shift = 0
    while True:
        b = data[i]; i += 1
        result |= (b & 0x7f) << shift
        if not (b & 0x80):
            return result
        shift += 7
while i < len(data):
    section = data[i]; i += 1
    size = uleb()
    end = i + size
    if section == 2:
        n = uleb()
        for _ in range(n):
            ml = uleb(); i += ml
            nl = uleb(); name = data[i:i+nl].decode('utf-8', 'replace'); i += nl
            kind = data[i]; i += 1
            if kind == 0:
                uleb()
            elif kind in (1, 2):
                flags = data[i]; i += 1
                uleb()
                if flags & 1:
                    uleb()
            elif kind == 3:
                i += 1
            if name == want:
                sys.exit(0)
        sys.exit(1)
    i = end
sys.exit(1)
PY
}

PCMANFM=""
MOUSEPAD=""
MOUSEPAD_STALE=()
choose_pcmanfm() {
  local candidate
  for candidate in "$ASSETS/pcmanfm.wasm" "$EXP/pcmanfm.wasm" "$EXP/third_party/pcmanfm-threads/src/pcmanfm"; do
    if [ -f "$candidate" ] && is_wasm_binary "$candidate"; then
      PCMANFM="$candidate"
      return 0
    fi
  done
  return 1
}
choose_mousepad() {
  local candidate
  for candidate in "$ASSETS/mousepad.wasm" "$EXP/mousepad.wasm" "$EXP/third_party/mousepad/mousepad/mousepad"; do
    if [ -f "$candidate" ] && is_wasm_binary "$candidate"; then
      if wasm_imports_name "$candidate" pthread_create; then
        MOUSEPAD_STALE+=("$candidate")
      else
        MOUSEPAD="$candidate"
        return 0
      fi
    fi
  done
  return 1
}
choose_pcmanfm || true
choose_mousepad || true

VM_TREES=()
stage_tree() {
  local script="$1" out="$2"
  if bash "$EXP/scripts/$script" "$out" >/dev/null 2>&1; then
    VM_TREES+=(--vm-tree "$out")
    return 0
  fi
  return 1
}
find "${VMFONTS:-/tmp/vmfonts}" -name '*.ttf' 2>/dev/null | grep -q . || bash "$EXP/scripts/prepare-fonts.sh" >/dev/null 2>&1 || true
stage_tree prepare-xftfonts.sh "${VMXFT:-/tmp/vmxft}"
stage_tree prepare-icons.sh "${VMICONS:-/tmp/vmicons}"
stage_tree stage-gschemas.sh "${VMSCHEMAS:-/tmp/vmschemas}"
stage_tree prepare-dbus-fixtures.sh "${VMDBUS:-/tmp/vmxu5sess}"
stage_tree prepare-xkb.sh "${VMXKB:-/tmp/vmxkb}" || {
  echo "WARNING: XKB keymap staging failed; keyboard input will not work until xkbcomp + xkeyboard-config are installed." >&2
}

stage_desktop_defaults() {
  local out="$1"
  rm -rf "$out"
  mkdir -p "$out/root/.config/Mousepad" "$out/root/.config/pcmanfm/default"
  cat > "$out/root/.config/Mousepad/settings.conf" <<EOF
[org/xfce/mousepad/preferences/window]
remember-size=true
remember-position=true
remember-state=true

[org/xfce/mousepad/state/window]
width=${SX_GUI_MOUSEPAD_WIDTH:-560}
height=${SX_GUI_MOUSEPAD_HEIGHT:-620}
left=${SX_GUI_MOUSEPAD_LEFT:-690}
top=${SX_GUI_MOUSEPAD_TOP:-50}
maximized=false
fullscreen=false
EOF
  cat > "$out/root/.config/pcmanfm/default/pcmanfm.conf" <<EOF
[ui]
win_width=${SX_GUI_PCMANFM_WIDTH:-600}
win_height=${SX_GUI_PCMANFM_HEIGHT:-620}
maximized=0
side_pane_mode=hidden;places
view_mode=list
EOF
  VM_TREES+=(--vm-tree "$out")
}
stage_desktop_defaults "${VMDESKTOP:-/tmp/vmwasm-desktop-defaults}"

echo "building wasm-gui-host (window feature) + sidecar..."
( cd "$REPO" && cargo build -p wasm-gui-host --features wasm-gui-host/window && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar ) || {
  echo "cargo build failed — a fresh checkout needs 'pnpm install' first (V8 bridge assets); see CLAUDE.md"; exit 1; }
HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"

for f in "$XVFB" "$TWM"; do
  [ -f "$f" ] || { echo "MISSING guest wasm: $f (expected the demo-assets bundle)"; exit 1; }
done
CLIENTS=(--client "$TWM")
MISSING_APPS=()
if [ -f "$PCMANFM" ]; then
  CLIENTS+=(--client "$PCMANFM --new-win /")
else
  MISSING_APPS+=("pcmanfm wasm not found; run experiments/wasm-gui/scripts/build-pcmanfm.sh")
fi
if [ -f "$MOUSEPAD" ]; then
  CLIENTS+=(--client "$MOUSEPAD")
else
  if [ "${#MOUSEPAD_STALE[@]}" -ne 0 ]; then
    MISSING_APPS+=("mousepad wasm candidates import pthread_create directly and are stale/broken; run experiments/wasm-gui/scripts/build-mousepad.sh")
  else
    MISSING_APPS+=("mousepad wasm not found; run experiments/wasm-gui/scripts/build-mousepad.sh")
  fi
fi
if [ "${#MISSING_APPS[@]}" -ne 0 ] && [ "$ALLOW_FALLBACK" != "1" ]; then
  echo "ERROR: cannot launch the default desktop because required app artifacts are missing or stale:"
  for app in "${MISSING_APPS[@]}"; do
    echo "  - $app"
  done
  echo "Set SX_GUI_ALLOW_FALLBACK=1 only when you intentionally want the xclock/demo-window fallback."
  exit 1
fi
if [ "${#MISSING_APPS[@]}" -ne 0 ]; then
  echo "WARNING: required app artifacts are missing or stale; using explicit SX_GUI_ALLOW_FALLBACK=1 fallback."
  [ -f "$XCLOCK" ] || { echo "MISSING guest wasm: $XCLOCK"; exit 1; }
  [ -f "$XWIN" ] || { echo "MISSING guest wasm: $XWIN"; exit 1; }
  CLIENTS+=(--client "$XCLOCK -analog -update 1 -geometry 220x220+920+60" --client "$XWIN")
fi

DBUS_ARGS=()
if [ -f "$DBUS" ]; then
  DBUS_ARGS+=(--dbus "$DBUS")
  [ -f "$XFCONFD" ] && DBUS_ARGS+=(--dbus-service "$XFCONFD")
fi

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(uname)" != "Darwin" ]; then
  echo "WARNING: no DISPLAY or WAYLAND_DISPLAY set — winit has no Linux display to open a window on."
  echo "         Run inside your desktop session, or headless via: xvfb-run -s \"-screen 0 ${W}x${H}x24\" $0"
fi

echo "launching interactive wasm desktop (${W}x${H}, ${SCALE}x native scale) — Esc or close the window to quit..."
# Do NOT override DISPLAY here: winit needs the HOST's real display (macOS native / your Linux X or
# Wayland). The ':0' inside the guest args below is the GUEST X server's display, which is separate.
exec "$HOST" --desktop \
  --desktop-scale "$SCALE" \
  --server "$XVFB" \
  ${DBUS_ARGS[@]+"${DBUS_ARGS[@]}"} \
  "${CLIENTS[@]}" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" ${VM_TREES[@]+"${VM_TREES[@]}"} --sidecar "$SIDECAR" \
  -- :0 -screen 0 "${W}x${H}x24" -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts
