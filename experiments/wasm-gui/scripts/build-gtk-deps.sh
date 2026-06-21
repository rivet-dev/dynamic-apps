#!/usr/bin/env bash
# M8: cross-compile the GTK rendering-stack dependencies to wasm32-wasip1, on top of the GLib stack
# (scripts/build-glib-stack.sh). Builds libpng, fribidi, and harfbuzz (the C++ text shaper), and
# installs GLib's devel artifacts + host code-gen tools so downstream libs (harfbuzz/pango/gtk) resolve.
#
# Built so far (this is the in-flight GTK port; cairo/pango/gdk-pixbuf/atk/gtk are the remaining steps):
#   GLib devel  -> prefix .pc + headers (meson install --tags devel) + glib-mkenums/genmarshal (host)
#   libpng      -> wasm-prefix/lib/libpng16.a (+ .pc, headers)
#   fribidi     -> wasm-prefix/lib/libfribidi.a (+ .pc, headers)
#   harfbuzz    -> wasm-prefix/lib/libharfbuzz.a (C++; needs libc++ + sjlj from the cross ini)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"
# Honor cross-env PREFIX (wasm-prefix vs wasm-prefix-threads); threaded build dirs + in-tree source dirs.
BD="build-wasm${SECURE_EXEC_WASM_THREADS:+-threads}"
SFX="${SECURE_EXEC_WASM_THREADS:+-threads}"
mkdir -p "$PREFIX/bin"

# 0) Ensure the GLib stack is built, then install its devel artifacts + host code-gen tools.
[ -f "$TP/glib/$BD/gio/libgio-2.0.a" ] || bash "$EXP/scripts/build-glib-stack.sh"
( cd "$TP/glib" && meson install -C "$BD" --tags devel --no-rebuild >/dev/null 2>&1 || true )
for t in glib-mkenums glib-genmarshal; do
  src="$(find "$TP/glib/$BD" -name "$t" -type f | head -1)"
  [ -n "$src" ] && { cp -f "$src" "$PREFIX/bin/$t"; chmod +x "$PREFIX/bin/$t"; }
done
echo "GLib devel + code-gen tools installed"

# 1) libpng (zlib already in prefix).
if [ ! -f "$PREFIX/lib/libpng16.a" ]; then
  cd "$TP"; [ -d "libpng$SFX" ] || { [ -f libpng.tar.gz ] || curl -fsSL -o libpng.tar.gz "https://download.sourceforge.net/libpng/libpng-1.6.43.tar.gz"; tar xf libpng.tar.gz && mv libpng-1.6.43 "libpng$SFX"; }
  cd "libpng$SFX" && CC="$CC" CFLAGS="$CFLAGS -I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" AR="$AR" RANLIB="$WSDK/bin/llvm-ranlib" \
    ./configure --host=wasm32-wasi --prefix="$PREFIX" --enable-static --disable-shared >/dev/null 2>&1
  make -j4 libpng16.la >/dev/null 2>&1 || true
  cp -f .libs/libpng16.a "$PREFIX/lib/"; mkdir -p "$PREFIX/include/libpng16"
  cp -f png.h pngconf.h pnglibconf.h "$PREFIX/include/" ; cp -f png.h pngconf.h pnglibconf.h "$PREFIX/include/libpng16/"
  find . -name libpng16.pc -exec cp -f {} "$PREFIX/lib/pkgconfig/" \; ; ln -sf libpng16.pc "$PREFIX/lib/pkgconfig/libpng.pc"
fi
echo "libpng: $(PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig pkg-config --modversion libpng 2>/dev/null)"

# 2) fribidi (pure C, meson).
if [ ! -f "$PREFIX/lib/libfribidi.a" ]; then
  cd "$TP"; [ -d fribidi ] || { curl -fsSL -o fribidi.tar.xz "https://github.com/fribidi/fribidi/releases/download/v1.0.13/fribidi-1.0.13.tar.xz"; tar xf fribidi.tar.xz && mv fribidi-1.0.13 fribidi; }
  cd fribidi && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" -Dtests=false -Ddocs=false -Dbin=false -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" install >/dev/null 2>&1
fi
echo "fribidi: $(PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig pkg-config --modversion fribidi 2>/dev/null)"

# 3) harfbuzz (C++; freetype + glib). Needs libc++ + -wasm-enable-sjlj (in the cross ini).
if [ ! -f "$PREFIX/lib/libharfbuzz.a" ]; then
  cd "$TP"; [ -d harfbuzz ] || { curl -fsSL -o harfbuzz.tar.xz "https://github.com/harfbuzz/harfbuzz/releases/download/8.5.0/harfbuzz-8.5.0.tar.xz"; tar xf harfbuzz.tar.xz && mv harfbuzz-8.5.0 harfbuzz; }
  cd harfbuzz && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
    -Dtests=disabled -Ddocs=disabled -Dutilities=disabled -Dcairo=disabled -Dglib=enabled -Dfreetype=enabled \
    -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" src/libharfbuzz.a >/dev/null 2>&1
  cp -f ${BD}/src/libharfbuzz.a "$PREFIX/lib/"; mkdir -p "$PREFIX/include/harfbuzz"
  cp -f src/hb*.h "$PREFIX/include/harfbuzz/" 2>/dev/null
  find "$BD" -name "harfbuzz*.pc" -exec cp -f {} "$PREFIX/lib/pkgconfig/" \;
fi
echo "harfbuzz: $(PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig pkg-config --modversion harfbuzz 2>/dev/null)"

# pkg-config fixes for the X/font .pc files (needed by cairo-xlib + pango):
#  - fontconfig.pc must pull expat (fcxml.o needs XML_*); add -lexpat to its public Libs.
#  - x11/xext/xrender/... require header-only X protocol packages (xproto/kbproto/...) whose .pc were
#    never installed; synthesize header-only stubs so transitive pkg-config resolution succeeds.
FCPC="$PREFIX/lib/pkgconfig/fontconfig.pc"
[ -f "$FCPC" ] && ! grep -q "lexpat" "$FCPC" && sed -i 's/^\(Libs: .*-lfontconfig\)$/\1 -lexpat/' "$FCPC"
for proto in xproto kbproto xextproto renderproto inputproto fixesproto fontsproto recordproto; do
  pc="$PREFIX/lib/pkgconfig/$proto.pc"
  [ -f "$pc" ] || printf 'prefix=%s\nincludedir=${prefix}/include\nName: %s\nDescription: X11 %s headers\nVersion: 2024.1\nCflags: -I${includedir}\n' "$PREFIX" "$proto" "$proto" > "$pc"
done

# 4) cairo (pixman+freetype+fontconfig+libpng; xlib backend). Needs compat sys/ipc.h+sys/shm.h (XShm).
if [ ! -f "$PREFIX/lib/libcairo.a" ]; then
  cd "$TP"; [ -d cairo ] || { curl -fsSL -o cairo.tar.xz "https://cairographics.org/releases/cairo-1.18.2.tar.xz"; tar xf cairo.tar.xz && mv cairo-1.18.2 cairo; }
  cd cairo && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
    -Dtests=disabled -Dxlib=enabled -Dxcb=disabled -Dfreetype=enabled -Dfontconfig=enabled \
    -Dpng=enabled -Dzlib=enabled -Dglib=enabled -Dquartz=disabled -Dtee=disabled -Dsymbol-lookup=disabled \
    -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" src/libcairo.a util/cairo-gobject/libcairo-gobject.a >/dev/null 2>&1
  meson install -C "$BD" --tags devel --no-rebuild >/dev/null 2>&1 || true
  cp -f ${BD}/src/libcairo.a "$PREFIX/lib/" 2>/dev/null
fi
echo "cairo: $(PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig pkg-config --modversion cairo 2>/dev/null)"

# 5) pango (cairo+harfbuzz+fribidi+glib+fontconfig).
if [ ! -f "$PREFIX/lib/libpango-1.0.a" ]; then
  cd "$TP"; [ -d pango ] || { curl -fsSL -o pango.tar.xz "https://download.gnome.org/sources/pango/1.52/pango-1.52.2.tar.xz"; tar xf pango.tar.xz && mv pango-1.52.2 pango; }
  cd pango && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
    -Dgtk_doc=false -Dintrospection=disabled -Dfontconfig=enabled -Dcairo=enabled \
    -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" pango/libpango-1.0.a pango/libpangocairo-1.0.a pango/libpangoft2-1.0.a >/dev/null 2>&1
  for l in libpango-1.0 libpangocairo-1.0 libpangoft2-1.0; do cp -f ${BD}/pango/$l.a "$PREFIX/lib/"; done
  find "$BD" -name "pango*.pc" -exec cp -f {} "$PREFIX/lib/pkgconfig/" \;
  mkdir -p "$PREFIX/include/pango-1.0/pango"; cp -f pango/*.h ${BD}/pango/*.h "$PREFIX/include/pango-1.0/pango/" 2>/dev/null
fi
echo "pango: $(PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig pkg-config --modversion pango 2>/dev/null)"
echo "== GTK rendering deps: libpng+fribidi+harfbuzz+cairo+pango built; gdk-pixbuf/atk/gtk remain =="
