#!/usr/bin/env bash
# M7 test: JWM (Joe's Window Manager), cross-compiled to wasm, runs as the desktop shell in one
# secure-exec VM and manages a real libX11 client window. Asserts JWM rendered its bottom panel/tray
# (taskbar + clock) and decorated the client window. A brand-name lightweight WM, all wasm.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"; JWMCFG="${VMJWM:-/tmp/vmjwm}"
FB="$(mktemp /tmp/m7-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/jwm.wasm" "$EXP/guest-xclient/xwin.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ]  || { echo "MISSING fonts $FONTS"; exit 1; }
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1
[ -d "$XFT" ]    || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFT" >/dev/null 2>&1
[ -d "$JWMCFG" ] || bash "$EXP/scripts/prepare-jwm.sh" "$JWMCFG" >/dev/null 2>&1

echo "== JWM (wasm) as the desktop shell, managing a libX11 window =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 28 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/jwm.wasm" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --vm-tree "$XFT" --vm-tree "$JWMCFG" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m7-run.log 2>&1 || true

python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
def nonblack(x0,y0,x1,y1):
    return sum(1 for y in range(y0,y1) for x in range(x0,x1)
               if pix[(y*W+x)*4:(y*W+x)*4+4].hex()!="00000000")
tray = nonblack(0, 454, W, H)          # JWM panel sits at the bottom of the screen
body = sum(1 for i in range(0,len(pix),4) if pix[i:i+4].hex()=="c0603000")  # xwin window managed
print(f"  JWM bottom panel nonblack px: {tray}   managed window body px: {body}")
assert tray > 3000, "JWM panel/taskbar did not render at the bottom of the screen"
assert body > 15000, "JWM did not map/manage the libX11 client window"
print("PASS: JWM (wasm) rendered its panel/taskbar/clock and managed a real libX11 window")
PY
rm -f "$FB"
echo "== M7 JWM PASS =="
