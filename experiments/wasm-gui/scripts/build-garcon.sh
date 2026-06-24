#!/usr/bin/env bash
# XU1: cross-compile UNMODIFIED garcon 4.18 (Xfce shared GTK widgets, a dep of xfce4-settings /
# xfsettingsd and the panel) to wasm32-wasip1-threads, static. Autotools, the proven Xfce recipe.
# Constraint #5: upstream untouched; wasi gaps in the platform layer (gettext stubs, glib-compat shim).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="4.18.2"
SRC="$TP/garcon"
[ -f "$PREFIX/lib/libxfce4util.a" ] || { echo "FATAL: libxfce4util not built"; exit 1; }
[ -f "$PREFIX/lib/libhostcompat.a" ] || bash "$EXP/scripts/build-libfm.sh" >/dev/null 2>&1 || true

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/garcon.tar.bz2" \
    "https://archive.xfce.org/src/xfce/garcon/4.18/garcon-4.18.2.tar.bz2" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf garcon.tar.bz2 && mv "garcon-$PV" garcon )
fi

cd "$SRC"
cp "$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)" \
   "$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)" . 2>/dev/null || true
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring garcon =="
# --with-vendor-info off; no gladeui/gtk-doc/introspection/vala; static.
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-gobject-introspection --disable-vala  \
  --without-x11-extras 2>/dev/null \
  > /tmp/conf-garcon.log 2>&1 || \
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls --disable-debug \
  --disable-gobject-introspection --disable-vala  \
  > /tmp/conf-garcon.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -30 /tmp/conf-garcon.log; exit 1; fi
echo "== building garcon =="
GLIBCOMPAT_O="$EXP/toolchain/glib-compat.o"
[ -f "$PREFIX/lib/libglibcompat.a" ] || { "$WSDK/bin/clang" --target=wasm32-wasip1-threads --sysroot="$WSDK/share/wasi-sysroot" -O2 -D_WASI_EMULATED_MMAN -DSECURE_EXEC_WASM_THREADS -pthread -c "$EXP/toolchain/glib-compat.c" -o "$GLIBCOMPAT_O" && "$WSDK/bin/llvm-ar" rcs "$PREFIX/lib/libglibcompat.a" "$GLIBCOMPAT_O"; }
make -j4 LDFLAGS="-L$PREFIX/lib -lglibcompat $LDFLAGS -ldbuscreds -Wl,--allow-undefined -Wl,--wrap=read -Wl,--wrap=getsockopt" > /tmp/make-garcon.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -45 /tmp/make-garcon.log; exit 1; fi
make install > /tmp/install-garcon.log 2>&1 || true
LIB=$(find . -name "libgarcon*.a" | head -1)
[ -n "$LIB" ] && cp -f "$LIB" "$PREFIX/lib/" && echo "OK: garcon built: $(basename "$LIB") ($(stat -c%s "$LIB") bytes)" || { echo "no .a produced"; exit 1; }
ls "$PREFIX/lib/pkgconfig/garcon"*.pc 2>/dev/null
