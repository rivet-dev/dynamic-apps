#!/usr/bin/env bash
# Automated M5 test: run the standard window manager twm + a real libX11 client window in one
# secure-exec VM (all cross-compiled to wasm), and assert the captured framebuffer shows the
# WM-decorated client window: the client's window body + content rectangles AND twm's title-bar
# decoration. Proves a standard, unmodified window manager manages a real toolkit client.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
TWM="$EXP/twm.wasm"
XWIN="$EXP/guest-xclient/xwin.wasm"
FB="$(mktemp /tmp/m5twm-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$XVFB" "$TWM" "$XWIN"; do
  [ -f "$f" ] || { echo "MISSING: $f (build it first)"; exit 1; }
done

echo "== running Xvfb + twm (WM) + a libX11 client window in one VM =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 30 \
  --server "$XVFB" --client "$TWM" --client "$XWIN" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > /tmp/m5twm-run.log 2>&1 || true

python3 - "$FB" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
pix = data[len(data)-640*480*4:]
# BGRX bytes: window body 0x3060C0->c0603000, green 0x20A020->20a02000, twm title highlight white
counts = {}
for name, hexv in [("window body","c0603000"), ("green rect","20a02000"),
                   ("client white","f0f0f000"), ("twm titlebar","ffffff00")]:
    counts[name] = sum(1 for i in range(0,len(pix),4) if pix[i:i+4].hex()==hexv)
for k,v in counts.items():
    print(f"  {k}: {v} px")
assert counts["window body"] > 20000, "client window not mapped by the WM"
assert counts["green rect"] > 3000 and counts["client white"] > 3000, "client did not draw into its window"
assert counts["twm titlebar"] > 500, "twm did not decorate the window with a title bar"
print("PASS: twm decorated and is managing the libX11 client window")
PY

rm -f "$FB"
echo "== M5 twm window-manager PASS =="
