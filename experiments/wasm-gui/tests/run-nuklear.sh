#!/usr/bin/env bash
# Automated M3 test: cross-compile the REAL Nuklear toolkit guest to wasm (wasi-sdk), run it THROUGH
# the secure-exec V8 sidecar via the Rust client, read back the framebuffer, validate header +
# golden pixels, emit a PNG proof. Headless; no display.
#
#   ./tests/run-nuklear.sh           verify against committed golden
#   ./tests/run-nuklear.sh --bless   (re)generate golden-nuklear.json
set -euo pipefail

cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
GUEST_WASM="$EXP/guest-nuklear/guest_nuklear.wasm"
HOST_BIN="$REPO/target/debug/wasm-gui-host"
SIDECAR_BIN="$REPO/target/debug/secure-exec-sidecar"
GOLDEN="$EXP/tests/golden-nuklear.json"
ASSETS="${PROOF_ASSETS:-$HOME/tmp/gui-progress/assets}"
BLESS="${1:-}"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
FRAME="$WORK/frame_nuklear.bin"
mkdir -p "$ASSETS"
emit() { printf '%s\n' "$*"; }

emit "## M3 automated test (real Nuklear toolkit through secure-exec) — $(date -u +%Y-%m-%dT%H:%M:%SZ)"

# 1. Build the Nuklear guest with wasi-sdk
emit "[1/6] building Nuklear guest (wasi-sdk -> wasm32-wasip1)…"
"$EXP/scripts/build-nuklear.sh" >/dev/null
[ -f "$GUEST_WASM" ] || { emit "FAIL: guest_nuklear.wasm not built"; exit 1; }
emit "build: guest_nuklear.wasm OK ($(stat -c%s "$GUEST_WASM") bytes)"

# 2. Ensure host + sidecar exist
emit "[2/6] ensuring host + sidecar…"
( cd "$REPO" && cargo build -p secure-exec-sidecar -p wasm-gui-host >/dev/null 2>&1 )
[ -x "$HOST_BIN" ] && [ -x "$SIDECAR_BIN" ] || { emit "FAIL: host/sidecar missing"; exit 1; }

# 3. Run the real toolkit THROUGH secure-exec
emit "[3/6] running Nuklear toolkit inside the secure-exec V8 sidecar…"
env -u DISPLAY -u WAYLAND_DISPLAY "$HOST_BIN" \
    --capture "$FRAME" --guest "$GUEST_WASM" --sidecar "$SIDECAR_BIN" 2>&1 | sed 's/^/  /'
[ -f "$FRAME" ] || { emit "FAIL: no frame captured"; exit 1; }

# 4. Header + golden pixels
emit "[4/6] validating header + golden pixels…"
if [ "$BLESS" = "--bless" ]; then
  node "$EXP/tests/check.mjs" "$FRAME" "$GOLDEN" --bless
fi
node "$EXP/tests/check.mjs" "$FRAME" "$GOLDEN" 2>&1 | sed 's/^/  /'

# 5. Exact frame size
EXPECT=$((12 + 640*480*4))
GOT=$(stat -c%s "$FRAME")
[ "$GOT" = "$EXPECT" ] || { emit "FAIL: frame size $GOT != $EXPECT"; exit 1; }
emit "frame size: $GOT bytes == 12 + 640*480*4 (exact)"

# 6. PNG proof
emit "[6/6] PNG proof…"
tail -c +13 "$FRAME" > "$WORK/raw.rgba"
ffmpeg -y -f rawvideo -pixel_format rgba -video_size 640x480 -i "$WORK/raw.rgba" \
  -frames:v 1 "$ASSETS/frame_nuklear.png" >/dev/null 2>&1
emit "proof PNG: $ASSETS/frame_nuklear.png"
emit "RESULT: PASS (real Nuklear toolkit rendered INSIDE secure-exec, via the Rust client)"
