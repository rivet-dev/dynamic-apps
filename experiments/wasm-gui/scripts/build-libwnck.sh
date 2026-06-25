#!/usr/bin/env bash
# XU2: cross-compile UNMODIFIED libwnck 3.24.1 (last autotools series; 3.32+ is meson) to
# wasm32-wasip1-threads, static. xfwm4 4.18 requires libwnck-3.0 >= 3.14 (the window/workspace list
# library); the previously staged libwnck was an ancient 3.4.9. Proven Xfce/GTK autotools recipe.
# Constraint #5: upstream untouched; wasi/platform gaps in the toolchain layer. Overwrites the old
# libwnck-3.a + libwnck-3.0.pc + headers in the prefix.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="$EXP/toolchain/host-bin:/home/linuxbrew/.linuxbrew/bin:$PATH"
export PERL5LIB="$EXP/toolchain/host-bin/perl5:${PERL5LIB:-}"
TP="$EXP/third_party"
PV="3.24.1"
SRC="$TP/libwnck324-threads"
[ -f "$PREFIX/lib/libgtk-3.a" ] || { echo "FATAL: gtk-3 not built"; exit 1; }
[ -f "$PREFIX/lib/libhostcompat.a" ] || { echo "FATAL: libhostcompat missing"; exit 1; }

if [ ! -d "$SRC" ]; then
  curl -fsSL -o "$TP/libwnck324.tar.xz" \
    "https://download.gnome.org/sources/libwnck/3.24/libwnck-$PV.tar.xz" || { echo "FETCH FAILED"; exit 1; }
  ( cd "$TP" && tar xf libwnck324.tar.xz && mv "libwnck-$PV" libwnck324-threads )
fi

cd "$SRC"
CFG_SUB="$(ls "$TP"/libX11-threads/config.sub 2>/dev/null | head -1)"
CFG_GUESS="$(ls "$TP"/libX11-threads/config.guess 2>/dev/null | head -1)"
for d in . build-aux; do
  [ -d "$d" ] && cp "$CFG_SUB" "$CFG_GUESS" "$d/" 2>/dev/null || true
done
make distclean >/dev/null 2>&1
export CC="$EXP/toolchain/clang-wasi-wrap.sh"
export CFLAGS="$CFLAGS -I$PREFIX/include"
export LDFLAGS="$LDFLAGS -L$PREFIX/lib -lhostcompat"
echo "== configuring libwnck $PV =="
./configure $CROSS_CONFIGURE_ARGS \
  --enable-static --disable-shared \
  --disable-gtk-doc --disable-gtk-doc-html --disable-nls \
  --disable-introspection --disable-startup-notification \
  > /tmp/conf-libwnck.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "CONFIGURE FAILED; tail:"; tail -30 /tmp/conf-libwnck.log; exit 1; fi
echo "== building libwnck =="
make -j4 LDFLAGS="-L$PREFIX/lib $LDFLAGS -Wl,--allow-undefined -Wl,--wrap=writev" > /tmp/make-libwnck.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "MAKE FAILED; tail:"; tail -45 /tmp/make-libwnck.log; exit 1; fi
make install > /tmp/install-libwnck.log 2>&1 || true
LIB=$(find . -name "libwnck-3.a" | head -1)
[ -n "$LIB" ] && cp -f "$LIB" "$PREFIX/lib/" && echo "OK: libwnck built: $(stat -c%s "$LIB") bytes" || { echo "no .a produced"; exit 1; }
echo "installed pc version: $(grep -i '^Version' "$PREFIX/lib/pkgconfig/libwnck-3.0.pc" 2>/dev/null)"
