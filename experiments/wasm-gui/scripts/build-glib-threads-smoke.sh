#!/usr/bin/env bash
# Threaded GLib smoke (WASM-THREADS-SPEC.md DoD 9.7): link guest-xclient/glib-threads-test.c against
# the THREADED GLib stack (third_party/glib/build-wasm-threads) and the threaded prefix deps, producing
# a wasi-threads guest. Proves GLib's own thread API works on the wasm-threads runtime.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
GB="$EXP/third_party/glib"; BD="$GB/build-wasm-threads"
[ -f "$BD/glib/libglib-2.0.a" ] || { echo "FATAL: threaded GLib not built (run build-glib-stack.sh with SECURE_EXEC_WASM_THREADS=1)"; exit 1; }
INC="-I$GB -I$GB/glib -I$BD -I$BD/glib"
OUT="$EXP/guest-xclient/glib-threads-test.wasm"
"$CC" $CFLAGS $INC -Wl,--allow-undefined -Wl,--no-check-features \
  -o "$OUT" "$EXP/guest-xclient/glib-threads-test.c" \
  "$BD/gthread/libgthread-2.0.a" "$BD/glib/libglib-2.0.a" \
  "$PREFIX/lib/libpcre2-8.a" "$PREFIX/lib/libz.a" "$PREFIX/lib/libintl.a" "$PREFIX/lib/libffi.a" \
  "$EXP/toolchain/wasi-compat-threads.o" \
  $LDFLAGS 2>&1 | grep -iE "error|undefined symbol" | head -20
[ -f "$OUT" ] && echo "built guest-xclient/glib-threads-test.wasm ($(stat -c%s "$OUT") bytes)" || { echo "LINK FAILED"; exit 1; }
