#!/usr/bin/env bash
# Generic threaded-GLib guest builder (WASM-THREADS-SPEC.md DoD §9): link a single
# guest-xclient/<name>.c against the THREADED GLib stack + threaded prefix deps -> wasi-threads guest.
# Generalizes build-glib-threads-smoke.sh to any source name. Usage: build-glib-test.sh <name>
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
NAME="${1:?usage: build-glib-test.sh <name>}"
GB="$EXP/third_party/glib"; BD="$GB/build-wasm-threads"
[ -f "$BD/glib/libglib-2.0.a" ] || { echo "FATAL: threaded GLib not built (build-glib-stack.sh w/ SECURE_EXEC_WASM_THREADS=1)"; exit 1; }
INC="-I$GB -I$GB/glib -I$BD -I$BD/glib"
OUT="$EXP/guest-xclient/$NAME.wasm"
"$CC" $CFLAGS $INC -Wl,--allow-undefined -Wl,--no-check-features \
  -o "$OUT" "$EXP/guest-xclient/$NAME.c" \
  "$BD/gthread/libgthread-2.0.a" "$BD/glib/libglib-2.0.a" \
  "$PREFIX/lib/libpcre2-8.a" "$PREFIX/lib/libz.a" "$PREFIX/lib/libintl.a" "$PREFIX/lib/libffi.a" \
  "$EXP/toolchain/wasi-compat-threads.o" \
  $LDFLAGS 2>&1 | grep -iE "error|undefined symbol" | head -20
[ -f "$OUT" ] && echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes)" || { echo "LINK FAILED"; exit 1; }
