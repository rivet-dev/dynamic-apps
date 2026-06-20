#!/usr/bin/env bash
# M6.1 (input) test: prove host-driven input reaches a real toolkit client through the wasm X server.
# twm manages an input-target client (blue; turns green on KeyPress, orange on ButtonPress). An XTEST
# agent guest, launched by the host, synthesizes a pointer button press over the target. Asserts the
# captured framebuffer shows the target repainted orange == the injected ButtonPress was delivered.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
FB="$(mktemp /tmp/m6inp-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/twm.wasm" \
         "$EXP/guest-xclient/xinput-target.wasm" "$EXP/guest-xclient/xtest-agent.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ] || { echo "MISSING fonts $FONTS"; exit 1; }
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1

echo "== twm + input-target + XTEST agent injecting a ButtonPress =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 30 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/guest-xclient/xinput-target.wasm" \
  --client "$EXP/guest-xclient/xtest-agent.wasm button 1 165 140" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m6inp-run.log 2>&1 || true

python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
def cnt(h): return sum(1 for i in range(0,len(pix),4) if pix[i:i+4].hex()==h)
blue=cnt("c0603000"); orange=cnt("2080e000"); green=cnt("20a02000")
print(f"  blue(initial)={blue}  orange(ButtonPress)={orange}  green(KeyPress)={green}")
assert orange > 10000, "injected ButtonPress was NOT delivered to the client (window not orange)"
assert blue < 5000, "client window still shows its initial colour (no input effect)"
print("PASS: XTEST-injected input reached a real libX11 client through the wasm X server")
PY
rm -f "$FB"
echo "== M6 input PASS =="
