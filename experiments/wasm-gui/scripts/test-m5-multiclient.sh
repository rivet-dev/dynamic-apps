#!/usr/bin/env bash
# M5 multi-client test: run the wasm Xvfb X server + THREE independent raw-X11 wasm clients in one
# secure-exec VM, each drawing a different-colored rectangle over AF_UNIX, and assert the shared
# server's framebuffer contains all three colors composited together.
set -euo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
XFILL="$EXP/guest-xclient/xfill.wasm"
FB="$(mktemp /tmp/m5-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$XVFB" "$XFILL"; do
  [ -f "$f" ] || { echo "MISSING: $f (build it first)"; exit 1; }
done

echo "== running Xvfb + 3 X clients in one VM =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 22 \
  --server "$XVFB" \
  --client "$XFILL 0 0 -1 -1 16746496" \
  --client "$XFILL 80 80 200 150 65280" \
  --client "$XFILL 360 250 200 150 255" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > /tmp/m5-run.log 2>&1 || true

python3 - "$FB" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
pix = data[len(data)-640*480*4:]
# BGRX bytes: orange 0xFF8800 -> 0088ff00, green 0x00FF00 -> 00ff0000, blue 0x0000FF -> ff000000
want = {"orange bg": b'\x00\x88\xff\x00', "green rect": b'\x00\xff\x00\x00', "blue rect": b'\xff\x00\x00\x00'}
counts = {k: 0 for k in want}
for i in range(0, len(pix), 4):
    px = pix[i:i+4]
    for k, v in want.items():
        if px == v:
            counts[k] += 1
for k, n in counts.items():
    print(f"  {k}: {n} px")
assert counts["orange bg"] > 100000, "background fill missing"
assert counts["green rect"] >= 25000, "green client did not render"
assert counts["blue rect"] >= 25000, "blue client did not render"
print("PASS: all three X clients rendered onto the shared wasm X server")
PY

rm -f "$FB"
echo "== M5 multi-client PASS =="
