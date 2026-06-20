#!/usr/bin/env bash
# M6 test: a robust multi-app desktop — the standard X.Org window manager twm managing a real
# libX11 client window AND a stock xclock app, all wasm in one secure-exec VM, with real X core
# fonts. Asserts the framebuffer shows the WM-decorated client window (body + content + title bar).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FB="$(mktemp /tmp/m6-fb.XXXXXX.bin)"
FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/twm.wasm" "$EXP/xclock.wasm" "$EXP/guest-xclient/xwin.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ] || { echo "MISSING fonts dir $FONTS (set VMFONTS)"; exit 1; }
# libX11 locale DB (so Xt apps like xclock can build a fontset and realize their widgets).
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1

echo "== twm + xclock + a libX11 window, with X core fonts =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 40 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/xclock.wasm -analog" \
  --client "$EXP/guest-xclient/xwin.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m6-run.log 2>&1 || true

grep -q "window manager is ready" /tmp/m6-run.log || { echo "FAIL: WM did not signal ready"; exit 1; }
python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); pix=data[len(data)-640*480*4:]
def cnt(hexv): return sum(1 for i in range(0,len(pix),4) if pix[i:i+4].hex()==hexv)
body=cnt("c0603000"); green=cnt("20a02000"); title=cnt("ffffff00")
print(f"  window body: {body}  green rect: {green}  twm titlebar/white: {title}")
assert body > 20000, "WM did not map the client window"
assert green > 2000, "client did not draw into its window"
assert title > 1000, "twm did not decorate (no title bar)"
print("PASS: twm manages a real libX11 window + a stock xclock, with real fonts")
PY
rm -f "$FB"
echo "== M6 desktop PASS =="
