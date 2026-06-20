#!/usr/bin/env bash
# Build the M3 Nuklear guest (real toolkit) to wasm32-wasip1 with the vendored wasi-sdk.
set -euo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
WSDK="${WASI_SDK:-$REPO/registry/native/c/vendor/wasi-sdk}"
[ -x "$WSDK/bin/clang" ] || { echo "wasi-sdk clang not found at $WSDK"; exit 1; }

"$WSDK/bin/clang" \
  --target=wasm32-wasip1 \
  --sysroot="$WSDK/share/wasi-sysroot" \
  -O2 -std=c99 \
  -I "$EXP/third_party/nuklear" \
  "$EXP/guest-nuklear/guest_nuklear.c" -lm \
  -o "$EXP/guest-nuklear/guest_nuklear.wasm"
echo "built guest-nuklear/guest_nuklear.wasm ($(stat -c%s "$EXP/guest-nuklear/guest_nuklear.wasm") bytes)"
