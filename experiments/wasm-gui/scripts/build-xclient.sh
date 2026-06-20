#!/usr/bin/env bash
# Build a single-file libX11 guest client (guest-xclient/<name>.c) to wasm: compile + link against the
# full cross-compiled X client stack + wasi-compat stubs + patched libc, then apply wasm-opt
# --fpcast-emu (the X libraries cast function pointers across signatures, which traps without it).
# Usage: build-xclient.sh <name-without-.c> [extra -l libs ...]
#   e.g. build-xclient.sh xinput-target
#        build-xclient.sh xtest-agent -lXtst
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
NAME="$1"; shift
EXTRA="$*"
SRC="$EXP/guest-xclient/$NAME.c"
OUT="$EXP/guest-xclient/$NAME.wasm"
P="$EXP/third_party/wasm-prefix/lib"
SETJMP="$WSDK/share/wasi-sysroot/lib/wasm32-wasip1/libsetjmp.a"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
COMPAT="$EXP/toolchain/wasi-compat.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

"$CC" $CFLAGS -I"$EXP/third_party/wasm-prefix/include" -I"$EXP/third_party/wasm-prefix/include/freetype2" $LDFLAGS -Wl,--allow-undefined \
  -o "$OUT" "$SRC" "$COMPAT" \
  -L"$P" $EXTRA -lXmu -lXt -lXext -lXrender -lX11 -lSM -lICE -lxcb -lXau -lXdmcp \
  "$SETJMP" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -20
if [ -f "$OUT" ]; then
  wasm-opt --fpcast-emu -O0 "$OUT" -o "$OUT.fp" 2>/dev/null && mv "$OUT.fp" "$OUT"
  echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes)"
else
  echo "BUILD FAILED ($NAME)"; exit 1
fi
