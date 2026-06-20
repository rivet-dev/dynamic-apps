#!/usr/bin/env bash
# Automated M1 test: build the wasm guest, run it THROUGH THE REAL secure-exec V8 sidecar via the
# standard Rust client host (no wasmer/node:wasi), read the framebuffer back through the client,
# validate the header, check golden pixels, and emit a PNG proof. Headless; no network.
#
#   ./tests/run.sh           verify against the committed golden
#   ./tests/run.sh --bless   (re)generate golden.json from the current build
set -euo pipefail

cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
GUEST_WASM="$EXP/guest/target/wasm32-wasip1/release/guest.wasm"
HOST_BIN="$REPO/target/debug/wasm-gui-host"
SIDECAR_BIN="$REPO/target/debug/secure-exec-sidecar"
GOLDEN="$EXP/tests/golden.json"
RESULTS="$EXP/tests/RESULTS.txt"
ASSETS="${PROOF_ASSETS:-$HOME/tmp/gui-progress/assets}"
BLESS="${1:-}"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
FRAME="$WORK/frame_secureexec.bin"
mkdir -p "$ASSETS"; : > "$RESULTS"
emit() { printf '%s\n' "$*" | tee -a "$RESULTS"; }
log() { printf '%s\n' "$*"; }

emit "## M1 automated test (through secure-exec) — $(date -u +%Y-%m-%dT%H:%M:%SZ)"

# 1. Build guest (standalone wasm workspace)
log "[1/7] building guest (wasm32-wasip1)…"
( cd "$EXP/guest" && cargo build --target wasm32-wasip1 --release >/dev/null 2>&1 )
[ -f "$GUEST_WASM" ] || { emit "FAIL: guest.wasm not built"; exit 1; }
emit "build: guest.wasm OK ($(stat -c%s "$GUEST_WASM") bytes)"

# 2. Build host + sidecar (root workspace, shared lock)
log "[2/7] building host + sidecar (root workspace)…"
( cd "$REPO" && cargo build -p secure-exec-sidecar -p wasm-gui-host >/dev/null 2>&1 )
[ -x "$HOST_BIN" ]    || { emit "FAIL: host binary missing"; exit 1; }
[ -x "$SIDECAR_BIN" ] || { emit "FAIL: sidecar binary missing"; exit 1; }
emit "build: wasm-gui-host + secure-exec-sidecar OK"

# 3. Capture a frame THROUGH secure-exec (guest runs inside the V8 sidecar)
log "[3/7] running guest through secure-exec V8 sidecar…"
env -u DISPLAY -u WAYLAND_DISPLAY "$HOST_BIN" \
    --capture "$FRAME" --guest "$GUEST_WASM" --sidecar "$SIDECAR_BIN" 2>&1 | sed 's/^/  /' | tee -a "$RESULTS"
[ -f "$FRAME" ] || { emit "FAIL: no frame captured through secure-exec"; exit 1; }
emit "secure-exec capture: $(stat -c%s "$FRAME") bytes"

# 4. Validate header + golden pixels on the raw framebuffer
log "[4/7] validating header + golden pixels…"
if [ "$BLESS" = "--bless" ]; then
  node "$EXP/tests/check.mjs" "$FRAME" "$GOLDEN" --bless
  emit "golden: blessed from secure-exec capture"
fi
node "$EXP/tests/check.mjs" "$FRAME" "$GOLDEN" 2>&1 | sed 's/^/  /' | tee -a "$RESULTS"

# 5. Sanity: exact byte size of a 640x480 RGBA frame + header
log "[5/7] frame size check…"
EXPECT=$((12 + 640*480*4))
GOT=$(stat -c%s "$FRAME")
[ "$GOT" = "$EXPECT" ] || { emit "FAIL: frame size $GOT != $EXPECT"; exit 1; }
emit "frame size: $GOT bytes == 12 + 640*480*4 (exact)"

# 6. PNG proof (human artifact only)
log "[6/7] encoding PNG proof…"
tail -c +13 "$FRAME" > "$WORK/raw.rgba"
ffmpeg -y -f rawvideo -pixel_format rgba -video_size 640x480 -i "$WORK/raw.rgba" \
  -frames:v 1 "$ASSETS/frame_secureexec.png" >/dev/null 2>&1
cp "$FRAME" "$ASSETS/frame_secureexec.bin"
emit "proof PNG: $ASSETS/frame_secureexec.png ($(stat -c%s "$ASSETS/frame_secureexec.png") bytes)"

# 7. Done
emit "RESULT: PASS (frame rendered by guest INSIDE secure-exec, via the standard Rust client)"
log "[7/7] PASS"
