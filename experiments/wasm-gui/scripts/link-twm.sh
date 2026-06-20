#!/usr/bin/env bash
# Link twm.wasm: the autotools build produces the objects but the final executable link needs the
# wasi-compat stub object + Xau/Xdmcp/setjmp + the patched libc (host_net sockets). Mirrors
# link-xvfb.sh. Produces experiments/wasm-gui/twm.wasm.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
SRC="$EXP/third_party/twm/src"
P="$EXP/third_party/wasm-prefix/lib"
SETJMP="$WSDK/share/wasi-sysroot/lib/wasm32-wasip1/libsetjmp.a"
LIBC="$REPO/registry/native/c/sysroot/lib/wasm32-wasip1/libc.a"
COMPAT="$EXP/toolchain/wasi-compat.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

OBJS="add_window.o cursor.o deftwmrc.o events.o gc.o iconmgr.o icons.o list.o menus.o parse.o resize.o session.o twm.o util.o version.o gram.o lex.o"

cd "$SRC"
"$CC" $CFLAGS $LDFLAGS \
  -Wl,--allow-undefined \
  -o twm $OBJS \
  "$COMPAT" \
  -L"$P" -lXmu -lXext -lXt -lX11 -lSM -lICE -lxcb -lXau -lXdmcp \
  "$SETJMP" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -20
RC=${PIPESTATUS[0]}

if [ -f twm ]; then
  cp twm "$EXP/twm.wasm"
  echo "linked twm.wasm ($(stat -c%s "$EXP/twm.wasm") bytes)"
else
  echo "LINK FAILED"; exit 1
fi
