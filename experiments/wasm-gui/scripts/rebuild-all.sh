#!/usr/bin/env bash
# Rebuild every cross-compiled dependency into wasm-prefix, in dependency order, after a working-copy
# reset wiped the large .a/.o outputs. Each autotools lib's exact ./configure invocation is recovered
# from its surviving config.log and re-run, so the precise per-lib flags are preserved. pixman (meson)
# and zlib (custom configure) are handled specially.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
TP="$EXP/third_party"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
ok=0; fail=0

autobuild() {  # autobuild <dir> [fallback configure args...]
  local d="$1"; shift
  [ -d "$TP/$d" ] || { echo "  SKIP $d (no dir)"; return; }
  cd "$TP/$d"
  local cfg
  cfg=$(grep -m1 '\$ ./configure' config.log 2>/dev/null | sed 's|.*\$ ./configure|./configure|')
  if [ -z "$cfg" ]; then
    # No prior configure recorded: bootstrap from configure.ac if needed, then use default flags.
    if [ ! -x ./configure ]; then
      ( NOCONFIGURE=1 ./autogen.sh >/dev/null 2>&1 || autoreconf -fi >/dev/null 2>&1 ) || true
    fi
    cfg="./configure $CROSS_CONFIGURE_ARGS $*"
  fi
  # Neutralize autotools maintainer-mode regen: the recovered Makefiles carry rules that re-run
  # aclocal-1.16/autoconf/automake (absent on this host) when timestamps look stale after a reset.
  local NOREGEN="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true AUTORECONF=true"
  ( eval "$cfg" && eval "make -j4 $NOREGEN" && eval "make install $NOREGEN" ) > "/tmp/rb-$d.log" 2>&1
  if [ $? -eq 0 ]; then echo "  OK   $d"; ok=$((ok+1)); else echo "  FAIL $d (/tmp/rb-$d.log)"; fail=$((fail+1)); fi
  cd "$EXP"
}

echo "== util-macros / protos =="
autobuild util-macros
# xorgproto is headers-only and must use meson (its autotools configure can't probe wasi fd_set).
if [ -d "$TP/xorgproto" ]; then
  ( cd "$TP/xorgproto" && rm -rf build-wasm && meson setup build-wasm --cross-file "$CROSS_INI" --prefix="$PREFIX" -Dlegacy=true >/dev/null 2>&1; ninja -C build-wasm install ) >/tmp/rb-xorgproto.log 2>&1 \
    && { echo "  OK   xorgproto"; ok=$((ok+1)); } || { echo "  FAIL xorgproto (/tmp/rb-xorgproto.log)"; fail=$((fail+1)); }
fi
autobuild xcbproto
autobuild font-util
autobuild libxtrans   # X transport headers (our patched Xtranssock.c recv/send lives here)

echo "== zlib (custom configure) =="
( cd "$TP/zlib" && CC="$CC" CFLAGS="$CFLAGS" AR="$AR" RANLIB="$RANLIB" ./configure --static --prefix="$PREFIX" \
  && make -j4 libz.a && make install ) >/tmp/rb-zlib.log 2>&1 \
  && { echo "  OK   zlib"; ok=$((ok+1)); } || { echo "  FAIL zlib (/tmp/rb-zlib.log)"; fail=$((fail+1)); }

echo "== pixman (meson) =="
if [ -d "$TP/pixman/build-wasm" ]; then
  ( cd "$TP/pixman" && ninja -C build-wasm && ninja -C build-wasm install ) >/tmp/rb-pixman.log 2>&1 \
    && { echo "  OK   pixman"; ok=$((ok+1)); } || { echo "  FAIL pixman (/tmp/rb-pixman.log)"; fail=$((fail+1)); }
fi

echo "== freetype =="
[ -x "$TP/freetype/configure" ] || ( cd "$TP/freetype" && sh autogen.sh >/dev/null 2>&1 ) || true
autobuild freetype --without-harfbuzz --without-png --without-brotli --without-bzip2 --with-zlib

echo "== libsha1-min =="
if [ -d "$TP/libsha1-min" ]; then
  ( cd "$TP/libsha1-min" && "$CC" $CFLAGS -c libsha1.c -o libsha1.o && "$AR" rcs "$PREFIX/lib/libsha1.a" libsha1.o && cp libsha1.h "$PREFIX/include/" ) >/tmp/rb-sha1.log 2>&1 \
    && { echo "  OK   libsha1-min"; ok=$((ok+1)); } || { echo "  FAIL libsha1-min"; fail=$((fail+1)); }
fi

HOSTTOOL="$PREFIX/lib/libhosttoolcompat.a"  # err()/getuid() for demo/test progs some libs build

echo "== X client libs (dependency order) =="
# libXaw needs Xt+Xmu+Xpm, so it must come AFTER the libXt/libXmu section below.
for d in libXau libxdmcp libxcb libX11 libXext libXrender libXfixes libXi libXtst libICE libSM; do
  autobuild "$d"
done

echo "== libXpm (src/ only; its sxpm tool links getuid, absent on wasi) =="
if [ -d "$TP/libXpm" ]; then
  cd "$TP/libXpm"
  cfg=$(grep -m1 '\$ ./configure' config.log 2>/dev/null | sed 's|.*\$ ./configure|./configure|')
  [ -n "$cfg" ] || cfg="./configure $CROSS_CONFIGURE_ARGS"
  N="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true"
  ( eval "$cfg" && eval "make -j4 -C src $N" && eval "make -C src install $N" \
      && eval "make -C include install $N" && eval "make install-pkgconfigDATA $N" ) >/tmp/rb-libXpm.log 2>&1 \
    && { echo "  OK   libXpm"; ok=$((ok+1)); } || { echo "  FAIL libXpm (/tmp/rb-libXpm.log)"; fail=$((fail+1)); }
  cd "$EXP"
fi

echo "== libXt (native makestrs first) =="
if [ -d "$TP/libXt" ]; then
  ( cd "$TP/libXt/util" && gcc -O2 -o makestrs makestrs.c && gcc -O2 -c makestrs.c -o makestrs.o && touch makestrs makestrs.o ) >/tmp/rb-makestrs.log 2>&1
  autobuild libXt
fi
autobuild libXmu
autobuild libXaw   # depends on Xt, Xmu, Xpm (all installed above)
autobuild libfontenc

echo "== libXfont2 (non-recursive automake; link compat lib so its test progs build) =="
if [ -d "$TP/libXfont2" ]; then
  cd "$TP/libXfont2"
  cfg=$(grep -m1 '\$ ./configure' config.log 2>/dev/null | sed 's|.*\$ ./configure|./configure|')
  [ -n "$cfg" ] || cfg="./configure $CROSS_CONFIGURE_ARGS --without-fop --without-xmlto"
  N="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true"
  # The library (.libs/libXfont2.a) builds fine; only the bundled test progs need host net/uid
  # symbols. Build just the lib, then install via targeted targets that skip the test executables.
  ( eval "$cfg" && eval "make -j4 libXfont2.la $N" \
      && eval "make install-libLTLIBRARIES install-pkgconfigDATA install-libXfontincludeHEADERS $N" ) >/tmp/rb-libXfont2.log 2>&1 \
    && { echo "  OK   libXfont2"; ok=$((ok+1)); } || { echo "  FAIL libXfont2 (/tmp/rb-libXfont2.log)"; fail=$((fail+1)); }
  cd "$EXP"
fi

autobuild libxkbfile
# libxcvt is only needed by the full Xorg/Xwayland servers (required: build_xorg). Our Xvfb-only
# build does not depend on it, so it is intentionally NOT rebuilt.

echo "== done: $ok ok, $fail fail; installed .a = $(ls "$PREFIX/lib/"*.a 2>/dev/null | wc -l) =="
