#!/usr/bin/env bash
# Build the M8 ffi_closure spike guest (guest-xclient/closure-spike.c) to wasm32-wasip1. Pure wasm: no
# host import, no V8 engine flag. It proves the trampoline-pool closure technique (pre-generated
# per-slot trampolines handed out as runtime closures). --export-table so the table is visible; NO
# --fpcast-emu (all trampolines share the (i32)->i32 signature, so indirect calls match exactly).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
OUT="$EXP/guest-xclient/closure-spike.wasm"
"$CC" $CFLAGS $LDFLAGS -Wl,--export-table -o "$OUT" "$EXP/guest-xclient/closure-spike.c" "$LIBC" \
  2>&1 | grep -iE "error|undefined" | head -20
[ -f "$OUT" ] && echo "built guest-xclient/closure-spike.wasm ($(stat -c%s "$OUT") bytes)" || { echo "BUILD FAILED"; exit 1; }
