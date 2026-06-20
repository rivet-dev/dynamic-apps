#!/usr/bin/env bash
# M6-INTERACTIVE (drag) test: the host drags a twm-managed window's titlebar via X11/XTEST and asserts
# the window actually moves. This exercises the full interactive path the live --desktop window uses:
# host -> X server host-backed socket -> XTEST motion/button -> twm interactive move. Requires twm's
# OpaqueMove (so twm does not XGrabServer during the move, which would block the host's XTEST stream).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
FB="$(mktemp /tmp/m6drag-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/twm.wasm" "$EXP/guest-xclient/xwin.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ] || { echo "MISSING fonts $FONTS"; exit 1; }
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1

echo "== host drags the window titlebar (160,71) -> (430,330) via X11/XTEST =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 32 \
  --server "$EXP/Xvfb.wasm" --client "$EXP/twm.wasm" --client "$EXP/guest-xclient/xwin.wasm" \
  --inject "h=motion 160 71" --inject "h=buttondn 1" \
  --inject "h=motion 250 180" --inject "h=motion 360 280" --inject "h=motion 430 330" --inject "h=buttonup 1" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m6drag-run.log 2>&1 || true

python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
xs=[]; ys=[]
for i in range(0,len(pix),4):
    if pix[i:i+4].hex()=="c0603000":  # xwin blue body
        p=i//4; xs.append(p%W); ys.append(p//W)
assert xs, "window body not found at all"
x0,y0 = min(xs), min(ys)
print(f"  window body top-left after drag: ({x0},{y0})  [initial map is ~(42,83)]")
assert x0 > 150 and y0 > 150, "window did not move to the drag destination (drag failed)"
print("PASS: host-driven X11/XTEST drag moved the twm-managed window")
PY
rm -f "$FB"
echo "== M6 drag PASS =="
