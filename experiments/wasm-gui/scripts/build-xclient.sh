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
PFX="$EXP/third_party/wasm-prefix${SECURE_EXEC_WASM_THREADS:+-threads}"; P="$PFX/lib"
WASMSUB="wasm32-wasip1${SECURE_EXEC_WASM_THREADS:+-threads}"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
COMPAT="$EXP/toolchain/wasi-compat${SECURE_EXEC_WASM_THREADS:+-threads}.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
# Threaded: kernel-backed sockets/pipe (vanilla threaded sysroot lacks them) + real -pthread libs.
if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
  HOSTOBJS="$EXP/toolchain/threads-libs/host_socket.o $EXP/toolchain/threads-libs/host_pipe_dup.o $EXP/toolchain/threads-libs/override_fcntl.o"
  EXTRALIB="-lwasi-emulated-signal"
else HOSTOBJS=""; EXTRALIB=""; fi

"$CC" $CFLAGS -I"$PFX/include" -I"$PFX/include/freetype2" $LDFLAGS -Wl,--allow-undefined \
  -o "$OUT" "$SRC" "$COMPAT" $HOSTOBJS \
  -L"$P" $EXTRA -lXmu -lXt -lXext -lXrender -lX11 -lSM -lICE -lxcb -lXau -lXdmcp $EXTRALIB \
  "$SETJMP" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -20
if [ -f "$OUT" ]; then
  wasm-opt --fpcast-emu -O0 "$OUT" -o "$OUT.fp" 2>/dev/null && mv "$OUT.fp" "$OUT"
  echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes)"
else
  echo "BUILD FAILED ($NAME)"; exit 1
fi
