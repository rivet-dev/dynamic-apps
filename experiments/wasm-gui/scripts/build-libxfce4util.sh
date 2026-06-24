#!/usr/bin/env bash
# XU1: cross-compile UNMODIFIED libxfce4util 4.18 (Xfce base-utils, a dep of xfconf + every Xfce
# component) to wasm32-wasip1-threads against the threaded GLib stack, static. Autotools (the 4.18
# tarball is configure-based, like dbus). Constraint #5: upstream source untouched; wasi gaps fixed
# in the platform layer.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
# Host build tools: the intltool stubs (configure needs intltool >= 0.35) + brew.
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.2"
SRC="$TP/libxfce4util"
GB="$TP/glib"; BD="$GB/build-wasm-threads"
[ -f "$BD/gio/libgio-2.0.a" ] || { echo "FATAL: threaded GLib/GIO not built"; exit 1; }
[ -f "$PREFIX/lib/libhostcompat.a" ] || bash "$EXP/scripts/build-libfm.sh" >/dev/null 2>&1 || true

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/libxfce4util.tar.bz2" \
    "https://archive.xfce.org/src/xfce/libxfce4util/4.18/libxfce4util-$PV.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf libxfce4util.tar.bz2 && mv "libxfce4util-$PV" libxfce4util )
fi
mkdir -p "$PREFIX/lib"
for lib in glib/libglib-2.0.a gobject/libgobject-2.0.a gthread/libgthread-2.0.a gmodule/libgmodule-2.0.a gio/libgio-2.0.a; do
  cp -f "$BD/$lib" "$PREFIX/lib/" 2>/dev/null || true
done

cd "$SRC"
cp "$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1
# Use the clang wrapper so wasm-ld-incompatible linker args libtool injects (--as-needed, ELF-isms)
# are stripped from the binary links (xfce4-kiosk-query, and later xfconfd/xfconf-query).
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
# Configure WITHOUT --allow-undefined for accurate feature detection; link -lhostcompat so the real
# host_net/compat symbols resolve. A static lib has no final exe link, so --allow-undefined is not
# needed here (added at the final link of the binaries that USE this lib).
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring libxfce4util =="
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls \
  --disable-debug \
  > /tmp/conf-libxfce4util.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -30 /tmp/conf-libxfce4util.log; exit 1; fi
echo "== building libxfce4util =="
# Binaries link glib (which carries undefined wasm host_net imports resolved at instantiation), so the
# final exe links need --allow-undefined; the wrapper strips --as-needed.
make -j4 LDFLAGS="$LDFLAGS -Wl,--allow-undefined" > /tmp/make-libxfce4util.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -40 /tmp/make-libxfce4util.log; exit 1; fi
make install > /tmp/install-libxfce4util.log 2>&1 || true
LIB=$(find . -name "libxfce4util*.a" | head -1)
[ -n "$LIB" ] && cp -f "$LIB" "$PREFIX/lib/" && echo "OK: libxfce4util built: $(basename "$LIB") ($(stat -c%s "$LIB") bytes)" || { echo "no .a produced"; exit 1; }
ls -la "$PREFIX/lib/pkgconfig/libxfce4util"*.pc 2>/dev/null
