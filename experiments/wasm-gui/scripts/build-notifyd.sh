#!/usr/bin/env bash
# XU6: cross-compile UNMODIFIED xfce4-notifyd 0.9.6 (the Xubuntu notification daemon) to
# wasm32-wasip1-threads. Same proven recipe as build-ristretto.sh: direct (non-libtool) final link
# (__wasi_init_tp), errno.o bundle, gio-vfs-local/empty-path shims, libxfce4ui GResource force-link.
# Builds the DAEMON (xfce4-notifyd/) which shows the notification popup window. Constraint #5.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"; SRC="$TP/xfce4-notifyd"
[ -d "$SRC" ] || { echo "FATAL: not fetched"; exit 1; }
[ -f "$PREFIX/lib/libnotify.a" ] || { echo "FATAL: libnotify not built"; exit 1; }

cd "$SRC"
if [ -f "$EXP/third_party/xfce4-notifyd.tar.bz2" ]; then
  TMP="$(mktemp -d)"; tar xjf "$EXP/third_party/xfce4-notifyd.tar.bz2" -C "$TMP"
  ( cd "$TMP"/xfce4-notifyd-* && find . \( -name 'Makefile.in' -o -name 'config.h.in' \) | tar -cf - -T - ) | tar -xf - -C "$SRC/"
  rm -rf "$TMP"
fi
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"; CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true; done
find . -name Makefile -type f -delete 2>/dev/null
rm -f config.status config.cache config.log xfce4-notifyd/xfce4-notifyd 2>/dev/null
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' \) -delete 2>/dev/null
find . -name '.libs' -type d -exec rm -rf {} + 2>/dev/null
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include -g0"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfce4-notifyd (daemon only; skip the panel plugin) =="
./configure $CROSS_CONFIGURE_ARGS --datadir=/usr/share --sysconfdir=/etc \
  --enable-static --disable-shared --disable-panel-plugin --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  > /tmp/conf-notifyd.log 2>&1
RC=$?; if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED:"; tail -30 /tmp/conf-notifyd.log; exit 1; fi
echo "configure OK"

WASMSUB="wasm32-wasip1-threads"; SETJMP="$WSDK/share/wasi-sysroot/lib/$WASMSUB/libsetjmp.a"
RESO="$EXP/toolchain/libxfce4ui-resources.o"
( cd /tmp && "$WSDK/bin/llvm-ar" x "$PREFIX/lib/libxfce4ui-2.a" libxfce4ui_2_la-libxfce4ui-resources.o 2>/dev/null && mv -f libxfce4ui_2_la-libxfce4ui-resources.o "$RESO" )
VFSSHIM="$EXP/toolchain/gio-vfs-local-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/gio-vfs-local-shim.c" -o "$VFSSHIM"
EMPTYSHIM="$EXP/toolchain/wasi-empty-path-shim.o"; "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -pthread -c "$EXP/toolchain/wasi-empty-path-shim.c" -o "$EMPTYSHIM"
ERRNOO="$EXP/toolchain/libc-errno.o"; ( cd /tmp && "$WSDK/bin/llvm-ar" x "$WSDK/share/wasi-sysroot/lib/$WASMSUB/libc.a" errno.o 2>/dev/null && mv -f errno.o "$ERRNOO" )
rm -f "$PREFIX/lib/libwasmshims.a"; "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libwasmshims.a" "$VFSSHIM" "$EMPTYSHIM" "$ERRNOO"

echo "== compiling notifyd daemon objects (libtool binary discarded) =="
rm -f "$SRC/xfce4-notifyd/xfce4-notifyd"
make -j4 -C xfce4-notifyd >> /tmp/make-notifyd.log 2>&1
echo "make rc=$? (compiles the .o)"
WRAPS="-Wl,--wrap=read -Wl,--wrap=getsockopt -Wl,--wrap=writev -Wl,--wrap=g_vfs_get_default -Wl,--wrap=open -Wl,--wrap=openat -Wl,--wrap=fopen -Wl,--wrap=stat -Wl,--wrap=lstat"
GTKLIBS=$(PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --static --libs gtk+-3.0 libxfce4ui-2 libxfconf-0 libnotify 2>/dev/null)
echo "== direct (non-libtool) final link =="
"$WSDK/bin/clang" $LDFLAGS -lhostcompat \
  "$SRC"/xfce4-notifyd/*.o \
  -o "$SRC/xfce4-notifyd/xfce4-notifyd" \
  -L"$PREFIX/lib" "$RESO" -lwasmshims -lglibcompat -ldbuscreds $GTKLIBS \
  -lXinerama -latk-bridge-2.0 -lepoxy -lXi -lXrandr -lXcursor -lXcomposite -lXdamage -lXfixes -lXtst -lXft -lXrender -lXext -lX11 -lXau -lXdmcp \
  "$SETJMP" -Wl,--allow-undefined $WRAPS -Wl,-z,stack-size=8388608 2>> /tmp/make-notifyd.log
RC=$?
echo "notifyd direct-link rc=$RC"
if [ -f "$SRC/xfce4-notifyd/xfce4-notifyd" ]; then
  echo "binary $(stat -c%s "$SRC/xfce4-notifyd/xfce4-notifyd") bytes; __wasi_init_tp defined: $("$WSDK/bin/llvm-nm" "$SRC/xfce4-notifyd/xfce4-notifyd" 2>/dev/null | grep -acawE '__wasi_init_tp')"
else echo "no binary"; tail -20 /tmp/make-notifyd.log; fi
