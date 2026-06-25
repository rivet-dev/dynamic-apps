#!/usr/bin/env bash
# XU6: cross-compile UNMODIFIED xfce4-appfinder 4.18 to wasm32-wasip1-threads (the application finder /
# launcher). Same Xfce autotools + GTK recipe as build-thunar.sh; deps (garcon/libxfce4ui/gtk/exo/xfconf)
# are already built. Constraint #5: upstream untouched; the gio-vfs-local + empty-path toolchain shims +
# the GResource force-link carry over. Also a test of whether a lighter GTK app than Thunar maps a window.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
SRC="$TP/xfce4-appfinder"
[ -d "$SRC" ] || { echo "FATAL: $SRC not fetched"; exit 1; }

cd "$SRC"
# Restore the generated dist files (Makefile.in, config.h.in) from the tarball; the shared tree churn wipes
# them. Only *.in templates so local edits survive.
if [ -f "$EXP/third_party/xfce4-appfinder.tar.bz2" ]; then
  echo "== restoring Makefile.in/config.h.in from xfce4-appfinder.tar.bz2 =="
  TMP="$(mktemp -d)"; tar xjf "$EXP/third_party/xfce4-appfinder.tar.bz2" -C "$TMP"
  ( cd "$TMP"/xfce4-appfinder-* && find . \( -name 'Makefile.in' -o -name 'config.h.in' \) | tar -cf - -T - ) | tar -xf - -C "$SRC/"
  rm -rf "$TMP"
fi
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
# Direct clean (no `make`; avoids the AM_MAINTAINER_MODE auto-regen that wipes Makefile.in).
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log src/xfce4-appfinder 2>/dev/null
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring appfinder =="
./configure $CROSS_CONFIGURE_ARGS \
  --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-appfinder.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -35 /tmp/conf-appfinder.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"
SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null \
  && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
GTKTRANS="$RESO $VFSSHIM $EMPTYSHIM -lxfce4ui-2 -lgarcon-gtk3-1 -lgarcon-1 -lexo-2 -lXinerama -latk-bridge-2.0 -latk-1.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp"
LINK="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,-z,stack-size=8388608"
echo "== building appfinder binary =="
rm -f "$SRC/src/xfce4-appfinder"
make -j4 -C src LDFLAGS="$LINK" LIBS="$GTKTRANS $SETJMP" >> /tmp/make-appfinder.log 2>&1
RC=$?
echo "appfinder make rc=$RC"
[ -f "$SRC/src/xfce4-appfinder" ] && echo "binary $(stat -c%s "$SRC/src/xfce4-appfinder") bytes" || echo "no binary"
if [ $RC -ne 0 ]; then echo "(tail:)"; tail -30 /tmp/make-appfinder.log; fi
