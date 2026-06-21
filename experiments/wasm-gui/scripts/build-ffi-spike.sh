#!/usr/bin/env bash
# Build the M8 FFI keystone spike guest (guest-xclient/ffi-spike.c) to wasm32-wasip1. It is tiny
# (libc only) and references the `host_net.ffi_call` import (--allow-undefined turns it into a wasm
# import). It needs `--export-table` so the runner can reach the guest's __indirect_function_table to
# call a function by index. NO --fpcast-emu: the test has no signature mismatches and fpcast would
# rewrite indirect-call indices, breaking the function-pointer == table-index assumption it relies on.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
OUT="$EXP/guest-xclient/ffi-spike.wasm"
"$CC" $CFLAGS $LDFLAGS -Wl,--allow-undefined -Wl,--export-table \
  -o "$OUT" "$EXP/guest-xclient/ffi-spike.c" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -20
[ -f "$OUT" ] && echo "built guest-xclient/ffi-spike.wasm ($(stat -c%s "$OUT") bytes)" || { echo "BUILD FAILED"; exit 1; }
