#!/usr/bin/env bash
# M6.1 (keyboard) test: prove host-driven KEYBOARD input reaches a real libX11 client through the wasm
# X server. This needs a working X keyboard device, which requires a compiled XKB keymap. wasi has no
# fork/exec so the server can't run xkbcomp; instead we install a host-precompiled keymap at
# /xkb/default.xkm and the server loads it (patched XkbCompileKeymap + fmemopen, no exec). twm manages
# an input-target client (blue; turns GREEN on KeyPress). The host injects a KeyPress via XTEST; the
# framebuffer must show the target repainted green == the key was translated and delivered.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"; VMXKB="${VMXKB:-/tmp/vmxkb}"
FB="$(mktemp /tmp/m6kbd-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/twm.wasm" \
         "$EXP/guest-xclient/xinput-target.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ] || { echo "MISSING fonts $FONTS"; exit 1; }
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1
[ -f "$VMXKB/xkb/default.xkm" ] || bash "$EXP/scripts/prepare-xkb.sh" "$VMXKB" >/dev/null 2>&1

echo "== twm + input-target; host injects a KeyPress via X11/XTEST (needs the precompiled keymap) =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 25 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/guest-xclient/xinput-target.wasm" \
  --inject "host=motion 165 140" --inject "host=button 1 165 140" --inject "host=key 38" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --vm-tree "$VMXKB" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m6kbd-run.log 2>&1 || true

grep -q "loaded precompiled keymap" /tmp/m6kbd-run.log || { echo "FAIL: server did not load the precompiled keymap"; exit 1; }
python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
def cnt(h): return sum(1 for i in range(0,len(pix),4) if pix[i:i+4].hex()==h)
green=cnt("20a02000")
print(f"  green(KeyPress)={green}")
assert green > 10000, "host XTEST KeyPress was NOT delivered/translated (window not green)"
print("PASS: host-driven keyboard input reached a real libX11 client through the wasm X server")
PY
rm -f "$FB"
echo "== M6 keyboard PASS =="
