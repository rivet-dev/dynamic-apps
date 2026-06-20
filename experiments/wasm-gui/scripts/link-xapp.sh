#!/usr/bin/env bash
# Link a cross-compiled X.Org Athena/Xt app (xclock, xcalc, ...) to wasm: the autotools build makes
# the objects but the final executable needs wasi-compat stubs + the full lib set + patched libc.
# Usage: link-xapp.sh <appdir-under-third_party> <output-name> <obj1.o obj2.o ...>
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
DIR="$1"; OUT="$2"; shift 2
OBJS="$*"
SRC="$EXP/third_party/$DIR"
P="$EXP/third_party/wasm-prefix/lib"
SETJMP="$WSDK/share/wasi-sysroot/lib/wasm32-wasip1/libsetjmp.a"
LIBC="$REPO/registry/native/c/sysroot/lib/wasm32-wasip1/libc.a"
COMPAT="$EXP/toolchain/wasi-compat.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

cd "$SRC"
"$CC" $CFLAGS $LDFLAGS -Wl,--allow-undefined \
  -o "$OUT" $OBJS "$COMPAT" \
  -L"$P" -lXaw7 -lXmu -lXt -lXpm -lXext -lXrender -lX11 -lSM -lICE -lxcb -lXau -lXdmcp \
  "$SETJMP" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -20
if [ -f "$OUT" ]; then
  wasm-opt --fpcast-emu -O0 "$OUT" -o "$OUT.fp" 2>/dev/null && mv "$OUT.fp" "$OUT"
  cp "$OUT" "$EXP/$OUT.wasm"
  echo "linked $OUT.wasm ($(stat -c%s "$EXP/$OUT.wasm") bytes)"
else
  echo "LINK FAILED ($OUT)"; exit 1
fi
