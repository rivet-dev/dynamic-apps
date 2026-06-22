#!/usr/bin/env bash
# M8 spike: link a GTK 3 application (guest-xclient/gtk-hello.c) against the full cross-compiled GTK
# stack (build-gtk3.sh) into a single wasm32-wasip1 guest, then size-optimize so it fits the runtime's
# frame + memory limits. Run on the wasm X server with: host --xdemo --client gtk-hello.wasm.
#
# Link notes:
#  - pkg-config --static --libs gtk+-3.0 pulls the whole stack; strip -pthread (clang-wasi-wrap also
#    drops it). --allow-undefined turns host_net socket calls into imports (like the other X guests);
#    --no-check-features tolerates the mixed atomics/bulk-memory features across GTK objects.
#  - emulated mman/process-clocks/pthread libs resolve mmap/clock/pthread_* (else they become env imports).
#  - --max-memory=128MiB: the runtime caps wasm memory max at 128 MiB; the module must declare <= that AND
#    a max greater than its initial so the heap can grow (GTK OOMs immediately with max==initial).
#  - wasm-opt --fpcast-emu (GTK casts fn pointers across signatures) with max-func-params@128 (GTK has
#    wide signatures), then -Oz to fit the 64 MiB transfer frame (54 MiB -> ~15 MiB). NOT --enable-threads
#    (that makes memory shared+fixed -> no heap growth).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
PREFIX="$EXP/third_party/wasm-prefix${SECURE_EXEC_WASM_THREADS:+-threads}"; P="$PREFIX/lib"
NAME="${1:-gtk-hello}"
WASMSUB="wasm32-wasip1${SECURE_EXEC_WASM_THREADS:+-threads}"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
COMPAT="$EXP/toolchain/wasi-compat${SECURE_EXEC_WASM_THREADS:+-threads}.o"
VANILLA="$WSDK/share/wasi-sysroot/lib/$WASMSUB"
OUT="$EXP/guest-xclient/$NAME.wasm"
# Threaded: real BSD sockets come from the patched host_socket.o (host_net ABI); the vanilla threaded
# sysroot has none. Also use real pthreads (-lwasi-emulated-signal), NOT the single-threaded emulation.
if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
  # host_socket.o (sockets) + host_pipe_dup.o (pipe/dup/dup2) route to the kernel host_net/host_fd ABI;
  # the vanilla threaded sysroot has neither. pipe() is required for GLib's cross-thread main-loop
  # wakeup (GWakeup), which real threads now exercise. Both objects are feature-agnostic (no atomics).
  HOSTSOCK="$EXP/toolchain/threads-libs/host_socket.o $EXP/toolchain/threads-libs/host_pipe_dup.o"
  THREAD_LIBS="-lwasi-emulated-signal"
  THREAD_LINK="-Wl,--shared-memory -Wl,--import-memory -Wl,--export-memory -Wl,--max-memory=$((512*1024*1024)) -Wl,--export=wasi_thread_start"
  MEMFLAG=""   # threaded memory is host-supplied (imported), growable via --max-memory above
else
  HOSTSOCK=""; THREAD_LIBS="-lwasi-emulated-pthread"; THREAD_LINK=""; MEMFLAG="-Wl,--max-memory=134217728"
fi

[ -f "$P/libgtk-3.a" ] || { echo "GTK not built; run scripts/build-gtk3.sh first"; exit 1; }
GFLAGS="$(PKG_CONFIG_LIBDIR="$P/pkgconfig" pkg-config --cflags gtk+-3.0)"
GLIBS="$(PKG_CONFIG_LIBDIR="$P/pkgconfig" pkg-config --static --libs gtk+-3.0 | sed 's/-pthread//g')"

echo "== linking $NAME against the GTK stack ($WASMSUB) =="
"$CC" $CFLAGS $GFLAGS -Wl,--allow-undefined -Wl,--no-check-features $MEMFLAG $THREAD_LINK \
  -o "$OUT" "$EXP/guest-xclient/$NAME.c" "$COMPAT" $HOSTSOCK $GLIBS \
  -L"$VANILLA" -lwasi-emulated-mman -lwasi-emulated-process-clocks $THREAD_LIBS \
  "$SETJMP" "$LIBC"
ENV_IMPORTS="$(wasm-dis "$OUT" 2>/dev/null | grep -coE '\(import "env" "')"
echo "linked ($(stat -c%s "$OUT") bytes); unresolved env imports: $ENV_IMPORTS (should be 0)"

echo "== fpcast-emu + size-optimize =="
# Threaded modules carry shared memory + atomics; --enable-threads keeps wasm-opt from rejecting/lowering
# them (memory is imported, so its growable limits are fixed at link, not by wasm-opt).
OPTFEAT="--enable-bulk-memory${SECURE_EXEC_WASM_THREADS:+ --enable-threads}"
wasm-opt --fpcast-emu -pa max-func-params@128 $OPTFEAT -O0 "$OUT" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers $OPTFEAT "$OUT.1" -o "$OUT"
rm -f "$OUT.1"
echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes); mem: $(wasm-dis "$OUT" 2>/dev/null | grep -oE '\(memory[^)]*\)' | head -1)"
