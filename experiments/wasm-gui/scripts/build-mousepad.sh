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
ERRNOO="$EXP/toolchain/libc-errno.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"
# NOTE: this errno.o force-bundle makes the link succeed, but it stops libc.a's crt from archive-pulling
# __init_tls.o, so __wasi_init_tp stays an undefined import -> instantiation LinkError (the render blocker).
# Bundling __init_tls.o instead duplicates it. The clean fix (next): pull errno from libc.a WITHOUT
# suppressing the crt TLS init (a correctly-passed -u errno, or providing __wasi_init_tp in the runtime).
# gtksourceview-4 + its libxml2 dep, then the GTK/X stack.
GTKTRANS="-lwasmshims -lgtksourceview-4 -lxml2 -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
# -Wl,-u,errno force-pulls libc.a's TLS errno.o definition; otherwise --allow-undefined synthesizes
# errno in non-TLS .bss and gtk/gtksourceview's TLS errno relocs (R_WASM_MEMORY_ADDR_TLS_SLEB) fail.
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,-u,errno -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,-z,stack-size=8388608"
echo "== building mousepad binary =="
rm -f "$SRC/mousepad/mousepad"
make -j4 -C mousepad LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-mousepad.log 2>&1
RC=$?
echo "mousepad make rc=$RC"
[ -f "$SRC/mousepad/mousepad" ] && echo "binary $(stat -c%s "$SRC/mousepad/mousepad") bytes" || echo "no binary"
if [ $RC -ne 0 ]; then echo "(tail:)"; tail -30 /tmp/make-mousepad.log; fi
