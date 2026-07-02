#!/usr/bin/env bash
# XU6: cross-compile UNMODIFIED mousepad 0.6.1 (the Xubuntu text editor) to wasm32-wasip1-threads. Same
# autotools+GTK recipe as build-appfinder.sh; the one extra dep gtksourceview-4 is now built. Uses the
# keyfile settings backend (--enable-keyfile-settings) to avoid dconf/gsettings (no dconf in the sandbox)
# + disables the dlopen plugin loader. Constraint #5: upstream untouched; the toolchain shims carry over.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
SRC="$TP/mousepad"
[ -d "$SRC" ] || { echo "FATAL: $SRC not fetched"; exit 1; }
[ -f "$PREFIX/lib/libgtksourceview-4.a" ] || { echo "FATAL: gtksourceview not built (run build-gtksourceview.sh)"; exit 1; }
stat_bytes() {
  stat -c%s "$1" 2>/dev/null || stat -f%z "$1"
}

cd "$SRC"
if [ -f "$EXP/third_party/mousepad.tar.bz2" ]; then
  echo "== restoring Makefile.in/config.h.in from mousepad.tar.bz2 =="
  TMP="$(mktemp -d)"; tar xjf "$EXP/third_party/mousepad.tar.bz2" -C "$TMP"
  ( cd "$TMP"/mousepad-* && find . \( -name 'Makefile.in' -o -name 'config.h.in' \) | tar -cf - -T - ) | tar -xf - -C "$SRC/"
  rm -rf "$TMP"
fi
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log mousepad/mousepad 2>/dev/null
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -I$PREFIX/include/gtksourceview-4 -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring mousepad =="
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --enable-keyfile-settings --disable-plugin-gspell --disable-plugin-shortcuts \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-mousepad.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-mousepad.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
# mousepad builds an intermediate libtool library (libmousepad.la) and libtool rejects raw .o in LIBS;
# archive the --wrap shims into a real .a and pass -lwasmshims (a library libtool accepts).
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
# Bundle libc.a's TLS errno.o INTO libwasmshims.a: gtk/gtksourceview reference errno as a direct TLS
# symbol, but errno.o (HIDDEN _Thread_local) is otherwise never archive-pulled, so --allow-undefined
# synthesizes errno in non-TLS .bss -> the TLS relocs fail. As an archive member, -lwasmshims pulls it
# to satisfy those references (libtool rejects a raw .o; -Wl,-u,errno got mangled).
# errno.o force-bundle: under -Wl,--allow-undefined gtk's direct TLS errno ref would otherwise be
# synthesized non-TLS. (KEY FINDING: dropping --allow-undefined entirely makes errno + __wasilibc_pthread_self
# resolve cleanly from libc.a as TLS, leaving __wasi_init_tp as the SINGLE undefined -- but its __init_tls.o
# is not archive-pulled by the crt here, and bundling __init_tls.o re-breaks errno. The render fix is to
# get that one __wasi_init_tp definition in without disturbing errno: provide it in the runtime imports,
# or determine why appfinder crt-pulls __init_tls.o and mousepad does not.)
ERRNOO="$EXP/toolchain/libc-errno.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"   # rebuild fresh (ar rcs only updates)
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"
# RENDER-BLOCKER (mousepad LINKS rc=0 but won't instantiate): __wasi_init_tp stays an undefined import.
# It is a THREE-WAY wasm-ld tension that resists a pure build-layer fix: (1) --allow-undefined is REQUIRED
# (libepoxy's GLX functions epoxy_glX* are legitimately unresolved -- software rendering, no GLX); but it
# also imports the HIDDEN _Thread_local libc symbols (errno, __wasi_init_tp, __wasilibc_pthread_self) as
# non-TLS. (2) Dropping it surfaces the epoxy GLX undefineds AND re-breaks errno. (3) __wasi_init_tp is
# referenced only via wasi_thread_start (gtksourceview spawns threads; appfinder does not), and its
# __init_tls.o is not archive-pulled here; forcing it (--undefined/bundle/2nd-libc.a) whack-a-moles into
# errno or __wasilibc_pthread_self. CLEAN FIX = provide __wasi_init_tp in the wasm-gui-host runtime imports
# (ONE symbol; the runtime already sets up instance TLS) -- a runtime/TCB change SURFACED for sign-off.
# gtksourceview-4 + its libxml2 dep, then the GTK/X stack.
GTKTRANS="-lwasmshims -lgtksourceview-4 -lxml2 -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
# -Wl,-u,errno force-pulls libc.a's TLS errno.o definition; otherwise --allow-undefined synthesizes
# errno in non-TLS .bss and gtk/gtksourceview's TLS errno relocs (R_WASM_MEMORY_ADDR_TLS_SLEB) fail.
LINK="-L$PREFIX/lib $LDFLAGS -Wl,--allow-undefined -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,-z,stack-size=8388608"
echo "== compiling mousepad objects (libtool link is discarded; see direct link below) =="
rm -f "$SRC/mousepad/mousepad"
make -j4 -C mousepad LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-mousepad.log 2>&1
echo "make rc=$? (compiles the .o; the libtool binary is discarded)"
# FINAL LINK: bypass libtool. The libtool final link nests libsetjmp.a inside libmousepad.a and never
# archive-pulls libc.a's __init_tls.o, so __wasi_init_tp stays an undefined import -> instantiation fails.
# A DIRECT clang link of the objects (exactly as appfinder's non-libtool link does) pulls __init_tls.o ->
# __wasi_init_tp is DEFINED. The full GTK closure comes from pkg-config (libmousepad_la-*.o = the app lib,
# mousepad-main.o = the binary's main).
WRAPS="-Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 gtksourceview-4 2>/dev/null)
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat \
  "$SRC"/mousepad/mousepad-main.o "$SRC"/mousepad/libmousepad_la-*.o \
  -o "$SRC/mousepad/mousepad" \
  -L"$PREFIX/lib" -lwasmshims -lgtksourceview-4 $GTKLIBS \
  -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-mousepad.log
RC=$?
echo "mousepad direct-link rc=$RC"
if [ -f "$SRC/mousepad/mousepad" ]; then
  echo "binary $(stat_bytes "$SRC/mousepad/mousepad") bytes; __wasi_init_tp defined: $("$WSDK/bin/llvm-nm" "$SRC/mousepad/mousepad" 2>/dev/null | grep -acawE '__wasi_init_tp')"
else echo "no binary"; tail -20 /tmp/make-mousepad.log; exit 1; fi
[ "$RC" -eq 0 ] || exit "$RC"

OUT="$EXP/mousepad.wasm"
wasm-opt --fpcast-emu -pa max-func-params@128 --enable-bulk-memory --enable-threads -O0 "$SRC/mousepad/mousepad" -o "$OUT.1"
wasm-opt -Oz --strip-debug --strip-dwarf --strip-producers --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT"
rm -f "$OUT.1"
echo "OK: mousepad.wasm ($(( $(stat_bytes "$OUT")/1024/1024 ))MB stripped)"
