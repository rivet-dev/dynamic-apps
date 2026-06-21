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

# Seed prefix .pc files that aren't produced by any lib build. libxcb's configure hard-requires the
# `pthread-stubs` pkg-config module; on the threaded profile we link real -pthread so the stub lib is
# empty (Cflags/Libs blank), but the .pc must exist for the dependency probe to pass.
if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
  mkdir -p "$PREFIX/lib/pkgconfig"
  if [ ! -f "$PREFIX/lib/pkgconfig/pthread-stubs.pc" ]; then
    cat > "$PREFIX/lib/pkgconfig/pthread-stubs.pc" <<'PC'
Name: pthread-stubs
Description: Pthread stubs (none needed: threaded wasi profile links real -pthread)
Version: 0.5
Libs:
Cflags:
PC
  fi
fi

autobuild() {  # autobuild <dir> [fallback configure args...]
  local base="$1"; shift
  # Threaded profile builds in a per-profile copy of the source (in-tree autotools) so it never reuses
  # the non-threaded config.status/objects; distclean the copy before reconfiguring.
  local d="$base${SECURE_EXEC_WASM_THREADS:+-threads}"
  if [ ! -d "$TP/$d" ]; then
    if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ] && [ -d "$TP/$base" ]; then cp -r "$TP/$base" "$TP/$d"; else echo "  SKIP $base (no dir)"; return; fi
  fi
  cd "$TP/$d"
  [ -n "${SECURE_EXEC_WASM_THREADS:-}" ] && ( make distclean >/dev/null 2>&1 || true )
  local cfg
  # Threaded profile: do NOT recover the non-threaded config.log invocation (it bakes in the
  # non-threaded --prefix/paths). Use a clean configure against the threaded $PREFIX from cross-env.
  if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then cfg=""; else
  cfg=$(grep -m1 '\$ ./configure' config.log 2>/dev/null | sed 's|.*\$ ./configure|./configure|')
  fi
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
PIXBD="build-wasm${SECURE_EXEC_WASM_THREADS:+-threads}"
if [ -d "$TP/pixman" ]; then
  # Threaded: fresh meson setup against the threaded cross-file/prefix (the non-threaded build-wasm
  # dir targets wasm32-wasip1 + the non-threaded prefix).
  ( cd "$TP/pixman" \
      && { [ -d "$PIXBD" ] || meson setup "$PIXBD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
             -Ddefault_library=static -Dtests=disabled -Ddemos=disabled -Dgtk=disabled \
             -Dlibpng=disabled -Dmmx=disabled -Dvmx=disabled -Dopenmp=disabled >/dev/null 2>&1; } \
      && ninja -C "$PIXBD" && ninja -C "$PIXBD" install ) >/tmp/rb-pixman.log 2>&1 \
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
XPMD="libXpm${SECURE_EXEC_WASM_THREADS:+-threads}"
if [ -d "$TP/libXpm" ]; then
  # Threaded: build in a per-profile copy and configure cleanly against the threaded $PREFIX (the
  # recovered config.log invocation bakes in the non-threaded prefix, which sent xpm.pc to the wrong
  # tree and broke libXaw's pkg-config probe).
  if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ] && [ ! -d "$TP/$XPMD" ]; then cp -r "$TP/libXpm" "$TP/$XPMD"; fi
  cd "$TP/$XPMD"
  if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
    ( make distclean >/dev/null 2>&1 || true ); cfg="./configure $CROSS_CONFIGURE_ARGS"
  else
    cfg=$(grep -m1 '\$ ./configure' config.log 2>/dev/null | sed 's|.*\$ ./configure|./configure|')
    [ -n "$cfg" ] || cfg="./configure $CROSS_CONFIGURE_ARGS"
  fi
  N="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true"
  ( eval "$cfg" && eval "make -j4 -C src $N" && eval "make -C src install $N" \
      && eval "make -C include install $N" && eval "make install-pkgconfigDATA $N" ) >/tmp/rb-$XPMD.log 2>&1 \
    && { echo "  OK   $XPMD"; ok=$((ok+1)); } || { echo "  FAIL $XPMD (/tmp/rb-$XPMD.log)"; fail=$((fail+1)); }
  cd "$EXP"
fi

echo "== libXt (native makestrs first) =="
if [ -n "${SECURE_EXEC_WASM_THREADS:-}" ]; then
  # Threaded: autobuild's distclean would wipe a pre-built makestrs, and the util/Makefile forces
  # CC=@CC_FOR_BUILD@ but still injects the cross $(CPPFLAGS) (--target) that native gcc rejects. So
  # configure first, build makestrs natively, then touch it newer than its source so `make` never
  # invokes the cross compiler on the build-time helper.
  if [ -d "$TP/libXt" ]; then
    [ -d "$TP/libXt-threads" ] || cp -r "$TP/libXt" "$TP/libXt-threads"
    ( cd "$TP/libXt-threads" && ( make distclean >/dev/null 2>&1 || true ) \
        && eval "./configure $CROSS_CONFIGURE_ARGS" \
        && cd util && gcc -O2 -c makestrs.c -o makestrs.o && gcc -O2 -o makestrs makestrs.o \
        && touch makestrs.c && sleep 1 && touch makestrs.o makestrs ) >/tmp/rb-makestrs.log 2>&1
    NOREGEN_XT="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true AUTORECONF=true"
    ( cd "$TP/libXt-threads" && eval "make -j4 $NOREGEN_XT" && eval "make install $NOREGEN_XT" ) >/tmp/rb-libXt-threads.log 2>&1 \
      && { echo "  OK   libXt-threads"; ok=$((ok+1)); } || { echo "  FAIL libXt-threads (/tmp/rb-libXt-threads.log)"; fail=$((fail+1)); }
  fi
elif [ -d "$TP/libXt" ]; then
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
