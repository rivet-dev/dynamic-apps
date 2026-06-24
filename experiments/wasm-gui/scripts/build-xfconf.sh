#!/usr/bin/env bash
# XU1: cross-compile UNMODIFIED xfconf 4.18 (the Xfce settings store: libxfconf client lib + xfconfd
# D-Bus daemon + xfconf-query CLI) to wasm32-wasip1-threads. xfconf talks to the session bus via GDBus,
# which now works over host_net. Autotools. Constraint #5: upstream untouched; wasi gaps in the
# platform layer (the GDBus host_net shims dbus_creds + the runtime socket-sharing/pipe-poll fixes).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.3"
SRC="$TP/xfconf"
GB="$TP/glib"; BD="$GB/build-wasm-threads"
[ -f "$PREFIX/lib/libxfce4util.a" ] || { echo "FATAL: libxfce4util not built (build-libxfce4util.sh)"; exit 1; }
[ -f "$PREFIX/lib/libdbuscreds.a" ] || { echo "FATAL: libdbuscreds.a missing (build-dbus.sh)"; exit 1; }

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/xfconf.tar.bz2" \
    "https://archive.xfce.org/src/xfce/xfconf/4.18/xfconf-$PV.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf xfconf.tar.bz2 && mv "xfconf-$PV" xfconf )
fi
mkdir -p "$PREFIX/lib"
for lib in glib/libglib-2.0.a gobject/libgobject-2.0.a gthread/libgthread-2.0.a gmodule/libgmodule-2.0.a gio/libgio-2.0.a; do
  cp -f "$BD/$lib" "$PREFIX/lib/" 2>/dev/null || true
done

cd "$SRC"
cp "$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring xfconf =="
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-gobject-introspection --disable-vala \
  > /tmp/conf-xfconf.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -30 /tmp/conf-xfconf.log; exit 1; fi
echo "== building xfconf =="
# glib-version-skew shim: the host gdbus-codegen (>= 2.84) emits g_variant_builder_init_static() in the
# generated bindings, but we link GLib 2.78 -> forward it to g_variant_builder_init().
GLIBCOMPAT_O="$EXP/toolchain/glib-compat.o"
"$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 \
  -D_WASI_EMULATED_MMAN -DSECURE_EXEC_WASM_THREADS -pthread \
  -c "$EXP/toolchain/glib-compat.c" -o "$GLIBCOMPAT_O" || exit 1
# Archive it (libtool rejects a raw .o in LDFLAGS when building the libxfconf .la, same as dbus_creds).
"$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libglibcompat.a" "$GLIBCOMPAT_O" || exit 1
# The binaries (xfconfd, xfconf-query) do GDBus over host_net: link the dbus_creds shims (host_net
# recvmsg/sendmsg/read/getsockopt(SO_PEERCRED)) + --wrap=read,getsockopt + --allow-undefined, exactly
# like the gdbus-probe. The wrapper strips --as-needed.
LINK_LDFLAGS="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt"
make -j4 LDFLAGS="$LINK_LDFLAGS" > /tmp/make-xfconf.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -45 /tmp/make-xfconf.log; exit 1; fi
make install LDFLAGS="$LINK_LDFLAGS" > /tmp/install-xfconf.log 2>&1 || true
echo "OK: xfconf built."
ls -la $(find . -name "xfconfd" -o -name "xfconf-query" -o -name "libxfconf*.a" 2>/dev/null | grep -vE "\.o$" | head -6) 2>/dev/null
