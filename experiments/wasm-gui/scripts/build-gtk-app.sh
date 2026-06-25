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
export CC="${CC:-$EXP/toolchain/clang-wasi-wrap.sh}"   # strips libtool/ELF-isms (--as-needed etc.)
PREFIX="$EXP/third_party/wasm-prefix${SECURE_EXEC_WASM_THREADS:+-threads}"; P="$PREFIX/lib"
NAME="${1:-gtk-hello}"
WASMSUB="wasm32-wasip1${SECURE_EXEC_WASM_THREADS:+-threads}"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
VANILLA="$WSDK/share/wasi-sysroot/lib/$WASMSUB"
OUT="$EXP/guest-xclient/$NAME.wasm"
# Threaded: real BSD sockets + the host-import/compat shims come from libhostcompat.a (host_socket +
# host_pipe_dup + override_fcntl + override_writev + wasi-compat-threads + ...), the SAME archive the
# Xfce/M8 GTK builds (build-lxpanel/openbox) link. The vanilla threaded sysroot has none of these.
if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
  [ -f "$P/libhostcompat.a" ] || { echo "libhostcompat.a missing; run scripts/build-openbox.sh (builds it)"; exit 1; }
  HOSTSOCK="-L$P -lhostcompat"
  THREAD_LIBS="-lwasi-emulated-signal"
  THREAD_LINK="-Wl,--shared-memory -Wl,--import-memory -Wl,--export-memory -Wl,--max-memory=$((512*1024*1024)) -Wl,--export=wasi_thread_start"
  MEMFLAG=""   # threaded memory is host-supplied (imported), growable via --max-memory above
else
  HOSTSOCK="$EXP/toolchain/wasi-compat.o"; THREAD_LIBS="-lwasi-emulated-pthread"; THREAD_LINK=""; MEMFLAG="-Wl,--max-memory=134217728"
fi

[ -f "$P/libgtk-3.a" ] || { echo "GTK not built; run scripts/build-gtk3.sh first"; exit 1; }
GFLAGS="$(PKG_CONFIG_LIBDIR="$P/pkgconfig" pkg-config --cflags gtk+-3.0)"
GLIBS="$(PKG_CONFIG_LIBDIR="$P/pkgconfig" pkg-config --static --libs gtk+-3.0 | sed 's/-pthread//g')"

# ★ Two MANDATORY GTK link flags (every GTK guest -- this was the xfsettingsd "Unable to open display"
# blocker): --wrap=writev (libxcb writes the X11 setup via writev, which MUST route to the host_net
# override in libhostcompat; unwrapped it hits wasi-libc's writev and silently fails on host_net fds,
# so gdk reports "Unable to open display") and an 8MB stack (GTK's deep init overflows the wasm default).
GTKFLAGS="-Wl,--wrap=writev -Wl,-z,stack-size=8388608"
echo "== linking $NAME against the GTK stack ($WASMSUB) =="
# EXTRA_LIBS: optional extra -l/-D flags for a probe (e.g. -lxfconf-0 -ldbuscreds + the GDBus wraps to
# exercise xfconf). Placed before $GLIBS so its symbols resolve against the GTK/GIO archives.
"$CC" $CFLAGS $GFLAGS -Wl,--allow-undefined -Wl,--no-check-features $GTKFLAGS $MEMFLAG $THREAD_LINK \
  -o "$OUT" "$EXP/guest-xclient/$NAME.c" ${EXTRA_LIBS:-} $GLIBS $HOSTSOCK \
  -L"$VANILLA" -lwasi-emulated-mman -lwasi-emulated-process-clocks $THREAD_LIBS \
  "$SETJMP" "$LIBC"
ENV_IMPORTS="$(wasm-dis "$OUT" 2>/dev/null | grep -coE '\(import "env" "')"
echo "linked ($(stat -c%s "$OUT") bytes); unresolved env imports: $ENV_IMPORTS (should be 0)"

echo "== fpcast-emu + size-optimize =="
# Threaded modules carry shared memory + atomics; --enable-threads keeps wasm-opt from rejecting/lowering
# them (memory is imported, so its growable limits are fixed at link, not by wasm-opt).
OPTFEAT="--enable-bulk-memory${SECURE_EXEC_WASM_THREADS:+ --enable-threads}"
# SECURE_EXEC_KEEP_NAMES=1 keeps the wasm name section (skip --strip-debug) so V8 --prof / the
# stack-dump can name guest functions. Larger binary; for diagnostics, not release.
# --debuginfo on the FPCAST pass preserves the linker's original C name section through fpcast-emu
# (without it, fpcast keeps only its own byn$fpcast-emu$N thunk names). KEEP_NAMES then -Oz with
# --debuginfo (size + names kept) so the binary still reproduces the same behaviour as release.
PASS1_DBG=""; [ -n "${SECURE_EXEC_KEEP_NAMES:-}" ] && PASS1_DBG="--debuginfo"
PA_ARG="-pa max-func-params@128"; [ -n "${SECURE_EXEC_FPCAST_NO_PA:-}" ] && PA_ARG=""
# DIAGNOSTIC: SECURE_EXEC_NO_FPCAST=1 drops --fpcast-emu entirely. Tests whether --fpcast-emu is itself
# breaking exact-signature vtable dispatch (GVfs/GFile) -- if g_file_new_for_path works without it, the
# fpcast pass is the culprit, not a genuine signature mismatch needing emulation.
FPCAST_ARG="--fpcast-emu $PA_ARG"; [ -n "${SECURE_EXEC_NO_FPCAST:-}" ] && FPCAST_ARG=""
wasm-opt $FPCAST_ARG $OPTFEAT $PASS1_DBG -O0 "$OUT" -o "$OUT.1"
if [ -n "${SECURE_EXEC_FPCAST0_ONLY:-}" ]; then
  # DIAGNOSTIC: skip the -Oz pass to test whether -Oz introduces the fpcast-emu GFile trap.
  cp -f "$OUT.1" "$OUT"
elif [ -n "${SECURE_EXEC_KEEP_NAMES:-}" ]; then
  wasm-opt -Oz --debuginfo $OPTFEAT "$OUT.1" -o "$OUT"
else
  wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers $OPTFEAT "$OUT.1" -o "$OUT"
fi
rm -f "$OUT.1"
echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes); mem: $(wasm-dis "$OUT" 2>/dev/null | grep -oE '\(memory[^)]*\)' | head -1)"
