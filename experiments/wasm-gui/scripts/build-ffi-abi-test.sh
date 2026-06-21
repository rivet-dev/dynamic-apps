#!/usr/bin/env bash
# Build the libffi-wasm shim ABI test: compile the shim (libffi-wasm/src/ffi.c) + the test
# (libffi-wasm/test/ffi-abi-test.c) to wasm32-wasip1. The shim's ffi_call uses the host_net.ffi_call
# import (--allow-undefined). --export-table so the host can reach __indirect_function_table. NO
# --fpcast-emu (ffi_call relies on function-pointer == table-index, and closures use an exact
# (i32,i32)->i32 trampoline signature, so indirect calls match without emulation).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
OUT="$EXP/guest-xclient/ffi-abi-test.wasm"
"$CC" $CFLAGS -I"$EXP/libffi-wasm/include" $LDFLAGS -Wl,--allow-undefined -Wl,--export-table \
  -o "$OUT" "$EXP/libffi-wasm/src/ffi.c" "$EXP/libffi-wasm/test/ffi-abi-test.c" "$LIBC" \
  2>&1 | grep -iE "error|undefined" | head -20
[ -f "$OUT" ] && echo "built guest-xclient/ffi-abi-test.wasm ($(stat -c%s "$OUT") bytes)" || { echo "BUILD FAILED"; exit 1; }
