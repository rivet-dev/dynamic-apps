#!/usr/bin/env bash
# Automated M4b test: run the cross-compiled wasm Xvfb X server + a raw-X11 wasm client in one
# secure-exec VM, have the client draw a solid fill, read the framebuffer back, and assert it
# contains the expected color. Exits non-zero on any failure.
set -euo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XVFB="$EXP/Xvfb.wasm"
XFILL="$EXP/guest-xclient/xfill.wasm"
FB="$(mktemp /tmp/m4b-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$XVFB" "$XFILL"; do
  [ -f "$f" ] || { echo "MISSING: $f (build it first)"; exit 1; }
done

echo "== running Xvfb + xfill in one VM =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 16 \
  --server "$XVFB" --client "$XFILL" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -fbdir /data > /tmp/m4b-run.log 2>&1 || true

echo "== checking results =="
grep -q "CMARK:done" /tmp/m4b-run.log || { echo "FAIL: client did not finish handshake+draw"; tail -20 /tmp/m4b-run.log; exit 1; }
grep -qE "1/1 X client\(s\) completed successfully" /tmp/m4b-run.log || { echo "FAIL: client exit non-zero"; tail -5 /tmp/m4b-run.log; exit 1; }

python3 - "$FB" <<'PY'
import sys, struct
data = open(sys.argv[1], 'rb').read()
assert len(data) >= 160 + 640*480*4, f"framebuffer too small: {len(data)}"
pix = data[len(data)-640*480*4:]
target = bytes([0x00, 0x88, 0xFF, 0x00])  # BGRX bytes for RGB 0xFF8800
match = sum(1 for i in range(0, len(pix), 4) if pix[i:i+4] == target)
frac = match / (640*480)
print(f"matching pixels: {match}/{640*480} = {frac:.3%}")
assert frac > 0.95, f"fill color coverage too low: {frac:.3%}"
print("PASS: framebuffer is filled with the X client's color")
PY

rm -f "$FB"
echo "== M4b PASS =="
