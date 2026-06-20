#!/usr/bin/env bash
# Build the minimal raw-X11 client (xfill) to wasm32-wasip1 against the PATCHED sysroot
# that provides POSIX sockets backed by secure-exec's host_net imports.
set -euo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
WSDK="$REPO/registry/native/c/vendor/wasi-sdk"
SYSROOT="$REPO/registry/native/c/sysroot"

"$WSDK/bin/clang" \
  --target=wasm32-wasip1 \
  --sysroot="$SYSROOT" \
  -O2 -std=c11 \
  "$EXP/guest-xclient/xfill.c" \
  -o "$EXP/guest-xclient/xfill.wasm"
echo "built guest-xclient/xfill.wasm ($(stat -c%s "$EXP/guest-xclient/xfill.wasm") bytes)"
