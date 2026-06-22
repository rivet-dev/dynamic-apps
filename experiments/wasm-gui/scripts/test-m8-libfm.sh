#!/usr/bin/env bash
# M8.3 acceptance: build the libfm-test guest and run it headless (no X) to prove the cross-compiled
# libfm + glib/gio data layer enumerates a kernel-VFS directory in wasm. PASS = libfm lists entries
# and exits 0. (The menu-cache freedesktop enumeration goes through menu-cache-gen which needs
# fork/exec at runtime — tracked separately; this proves the libfm folder/listing path.)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -f "$PREFIX/lib/libfm.a" ] || bash "$EXP/scripts/build-libfm.sh" || { echo "libfm build failed"; exit 1; }

WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"; LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
HOSTOBJS="$EXP/toolchain/threads-libs/host_socket.o $EXP/toolchain/threads-libs/host_pipe_dup.o $EXP/toolchain/threads-libs/override_fcntl.o"
PCCF=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --cflags libfm gio-2.0)
PCLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --libs libfm gio-2.0)
"$CC" $CFLAGS $PCCF -I"$PREFIX/include" -c "$EXP/guest-xclient/libfm-test.c" -o /tmp/libfm-test.o || exit 1
"$CC" $LDFLAGS -Wl,--allow-undefined -o "$EXP/libfm-test.wasm" /tmp/libfm-test.o \
  "$EXP/toolchain/wasi-compat-threads.o" $HOSTOBJS $PCLIBS -lhostcompat -lwasi-emulated-signal "$SETJMP" "$LIBC" || exit 1
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
wasm-opt --fpcast-emu --enable-bulk-memory --enable-threads -O0 "$EXP/libfm-test.wasm" -o /tmp/lf.fp && mv /tmp/lf.fp "$EXP/libfm-test.wasm"
echo "built libfm-test.wasm ($(stat -c%s "$EXP/libfm-test.wasm")b)"

out="$(env -u DISPLAY "$HOST" --exec --guest "$EXP/libfm-test.wasm" --timeout 35 --sidecar "$SIDECAR" 2>&1)"
echo "$out" | grep -E "LIBFM-TEST" | grep -v NETWRITE
if echo "$out" | grep -q "LIBFM-TEST: PASS"; then echo "M8.3 LIBFM: PASS"; exit 0; else echo "M8.3 LIBFM: FAIL"; exit 1; fi
