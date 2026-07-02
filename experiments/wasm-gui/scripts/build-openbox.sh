#!/usr/bin/env bash
# M8.2: cross-compile the openbox window manager (the LXDE WM) to wasm32-wasip1-threads and run it on
# the wasm X server. openbox builds from UNMODIFIED upstream source (constraint #5): every wasi gap is
# resolved in the platform/toolchain layer (libhostcompat.a + compat-include/grp.h + openbox-compat.c +
# the runtime's wasi fd_renumber), never by patching openbox.
#
# Platform pieces this relies on (all outside openbox):
#   - toolchain/openbox-compat.c : getpwuid/getpwuid_r (sandbox has no /etc/passwd; openbox derefs
#       pw->pw_name), pthread_exit (omitted by threaded wasi libc), alarm/gethostbyaddr stubs.
#   - toolchain/compat-include/grp.h : declares the group-enum API (setgrent/getgrent/endgrent) wasi-libc
#       omits, so callers don't get implicit-int signatures that mismatch the stubs and trap.
#   - libhostcompat.a : host_socket/host_pipe_dup/override_fcntl + wasi-compat-threads + openbox-compat,
#       bundled as a real archive (libtool rejects raw .o in its library stage); --whole-archive forces
#       the strong getpwuid to win over wasi-compat's weak NULL stub.
#   - crates/execution: wasi fd_renumber (Node WASI omits it; openbox imports it via libSM/dup).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
TP="$EXP/third_party"; WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
LIBC="$THREADS_SYSROOT/lib/$WASMSUB/libc.a"
TL="$EXP/toolchain/threads-libs"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

newest_config_sub() { ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1; }

# --- 1. libxml2 (openbox's only genuinely-new dependency closure; pure C, no net) ---
if [ ! -f "$PREFIX/lib/libxml2.a" ]; then
  cd "$TP"; [ -d libxml2 ] || { curl -fsSL -o libxml2.tar.xz \
    "https://download.gnome.org/sources/libxml2/2.9/libxml2-2.9.14.tar.xz"; tar xf libxml2.tar.xz && mv libxml2-2.9.14 libxml2; }
  [ -d libxml2-threads ] || cp -r libxml2 libxml2-threads
  cp "$(newest_config_sub)" "$(dirname "$(newest_config_sub)")/config.guess" libxml2-threads/ 2>/dev/null
  cd libxml2-threads
  ./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode \
    --without-python --without-http --without-ftp --without-zlib --without-lzma \
    --without-iconv --without-threads --without-modules --without-catalog >/tmp/conf-libxml2.log 2>&1
  touch aclocal.m4 configure config.h.in Makefile.in */Makefile.in 2>/dev/null
  make -j4 libxml2.la >/tmp/make-libxml2.log 2>&1 && make install >/tmp/inst-libxml2.log 2>&1 \
    && echo "  OK libxml2" || { echo "  FAIL libxml2"; exit 1; }
fi

# --- 2. pangoxft (openbox's PANGO check requires the Xft backend; our pango build skipped it) ---
if [ ! -f "$PREFIX/lib/libpangoxft-1.0.a" ]; then
  BD="$TP/pango/build-wasm-threads"; cd "$TP/pango" && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
    -Dgtk_doc=false -Dintrospection=disabled -Dfontconfig=enabled -Dcairo=enabled -Dxft=enabled \
    -Ddefault_library=static >/tmp/pango-setup.log 2>&1
  ninja -C "$BD" pango/libpango-1.0.a pango/libpangocairo-1.0.a pango/libpangoft2-1.0.a pango/libpangoxft-1.0.a >/tmp/pango-ninja.log 2>&1
  for l in libpango-1.0 libpangocairo-1.0 libpangoft2-1.0 libpangoxft-1.0; do cp -f "$BD/pango/$l.a" "$PREFIX/lib/"; done
  find "$BD" -name "pango*.pc" -exec cp -f {} "$PREFIX/lib/pkgconfig/" \; ; echo "  OK pangoxft"
fi

# --- 3. libhostcompat.a (platform host-import shims + compat stubs as a real archive) ---
"$CC" $CFLAGS -c "$EXP/toolchain/openbox-compat.c" -o "$EXP/toolchain/openbox-compat.o"
# wasi-compat-threads.o + override_fcntl.o are gitignored (**/*.o) build artifacts that a concurrent
# session in the shared workspace deletes; regenerate them here from source so the `ar` below never
# aborts mid-archive (which would silently leave a stale libhostcompat.a missing the host-import shims).
# override_fcntl.o is compiled from the platform libc fcntl override (the host_net socket-flag handling
# that lets libxcb use STOCK upstream fcntl — constraint #5).
# Compile against the vanilla WSDK threaded sysroot (NOT $CFLAGS' patched sysroot, which redeclares
# getrlimit and conflicts with wasi-compat.c's stub).
# -matomics -mbulk-memory: WITHOUT them, direct `errno` references (e.g. host_socket.c/host_pipe_dup.c)
# emit NON-TLS errno relocs, which force errno into non-TLS .bss and break any TLS-errno object pulled
# into the same link (gtk's gtkbuilder.c.o etc.). The threaded errno is TLS, so all objects must agree.
_COMPAT_CC=("$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 \
  -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -DSECURE_EXEC_WASM_THREADS -pthread -matomics -mbulk-memory)
mkdir -p "$TL" "$PREFIX/lib"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$EXP/toolchain/wasi-compat.c" -o "$EXP/toolchain/wasi-compat-threads.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$TL/host_socket.c" -o "$TL/host_socket.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$TL/host_pipe_dup.c" -o "$TL/host_pipe_dup.o"
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/fcntl.c" -o "$TL/override_fcntl.o"
# override_ioctl.o: host_net FIONREAD so libX11 uses STOCK upstream ioctl(FIONREAD) — constraint #5.
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/ioctl.c" -o "$TL/override_ioctl.o"
# override_writev.o: host_net writev (looped send) so libxcb uses STOCK upstream writev — needs the
# -Wl,--wrap=writev on the final link below; for non-host_net fds it delegates to __real_writev.
"${_COMPAT_CC[@]}" -I"$EXP/toolchain/compat-include" -c "$REPO/registry/native/patches/wasi-libc-overrides/writev_hostnet.c" -o "$TL/override_writev.o"
"$AR" rcs "$PREFIX/lib/libhostcompat.a" "$TL/host_socket.o" "$TL/host_pipe_dup.o" "$TL/override_fcntl.o" \
  "$TL/override_ioctl.o" "$TL/override_writev.o" "$EXP/toolchain/wasi-compat-threads.o" "$EXP/toolchain/openbox-compat.o"
echo "  OK libhostcompat.a"

# Fix stale non-threaded prefix paths baked into threaded .la dependency_libs (libtool would otherwise
# pull the non-threaded libX11 -> duplicate symbols).
sed -i 's#/third_party/wasm-prefix/lib#/third_party/wasm-prefix-threads/lib#g' "$PREFIX"/lib/*.la 2>/dev/null

# --- 4. openbox ---
cd "$TP"; [ -d openbox ] || { curl -fsSL -o openbox.tar "http://openbox.org/dist/openbox/openbox-3.6.1.tar.xz"; mkdir -p openbox && tar xf openbox.tar -C openbox --strip-components=1; }
[ -d openbox-threads ] || cp -r openbox openbox-threads
cp "$(newest_config_sub)" "$(dirname "$(newest_config_sub)")/config.guess" openbox-threads/ 2>/dev/null
cd openbox-threads
export LDFLAGS="$LDFLAGS -Wl,--whole-archive -lhostcompat -Wl,--no-whole-archive -Wl,--allow-undefined -Wl,--wrap=writev"
make distclean >/dev/null 2>&1
./configure $CROSS_CONFIGURE_ARGS --disable-maintainer-mode --disable-nls --disable-xinerama >/tmp/conf-openbox.log 2>&1
[ -f Makefile ] || { echo "  FAIL openbox configure"; tail -8 /tmp/conf-openbox.log; exit 1; }
touch aclocal.m4 configure config.h.in Makefile.in 2>/dev/null
# SETJMP+LIBC at the very END (LIBS) so the setjmp intrinsics + pthread_exit resolve after all objects.
make -j4 LIBS="$SETJMP $LIBC" >/tmp/make-openbox.log 2>&1 \
  && echo "  OK openbox/openbox ($(stat -c%s openbox/openbox) bytes)" || { echo "  FAIL openbox make"; tail -12 /tmp/make-openbox.log; exit 1; }

# --- 5. fpcast-emu (openbox casts fn-ptrs across signatures in its action dispatch) ---
# --strip-debug/--strip-dwarf: the linked X/glib libs carry DWARF that bloats the module ~3-4x
# (openbox 21MB -> 6MB), which slows V8 JIT and, with multiple guests, worsens the cross-process
# X-latency starvation. Strip it (runtime needs no debug info).
wasm-opt --fpcast-emu --strip-debug --strip-dwarf --enable-bulk-memory --enable-threads -O0 openbox/openbox -o "$EXP/openbox.wasm" 2>/dev/null
echo "OK: openbox.wasm ($(stat -c%s "$EXP/openbox.wasm") bytes). Stage config/theme with prepare-openbox-fixtures.sh."
