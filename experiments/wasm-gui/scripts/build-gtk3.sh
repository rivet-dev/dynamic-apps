#!/usr/bin/env bash
# M8: cross-compile GTK 3.24 (and its last deps: gdk-pixbuf, atk, the X libs GTK needs) to
# wasm32-wasip1, on top of build-glib-stack.sh + build-gtk-deps.sh (GLib/cairo/pango/harfbuzz/...).
# Produces third_party/gtk3/build-wasm/gtk/libgtk-3.a — the full GTK toolkit for wasm.
#
# Key cross-build mechanics handled here:
#  - gdk-pixbuf (png only) + atk + a STUB at-spi2 atk-bridge (no AT-SPI/D-Bus in the sandbox).
#  - the X libs GTK needs but the X stack didn't build: Xrandr, Xcursor, Xcomposite, Xdamage
#    (+ header-only proto .pc stubs: randrproto/compositeproto/damageproto).
#  - HOST code-gen tools: glib-compile-resources/schemas come from the host (native); gdbus-codegen is
#    wrapped to run OUR GLib 2.78.4 codegen module (the host's newer gdbus-codegen emits APIs our target
#    GLib lacks). glib-mkenums/genmarshal are the GLib-2.78.4 python scripts.
#  - GTK pulls libepoxy as a meson subproject; wayland/introspection/demos/tests/colord off.
set -euo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"; PREFIX="$TP/wasm-prefix${SECURE_EXEC_WASM_THREADS:+-threads}"; PCDIR="$PREFIX/lib/pkgconfig"
# Threaded profile builds in per-profile meson dirs / source copies so it never mixes with the
# non-threaded artifacts (BD = meson build dir, SFX = autotools source-copy suffix, GBD = GLib build dir).
BD="build-wasm${SECURE_EXEC_WASM_THREADS:+-threads}"
SFX="${SECURE_EXEC_WASM_THREADS:+-threads}"
GBD="build-wasm${SECURE_EXEC_WASM_THREADS:+-threads}"
mkdir -p "$PREFIX/bin"

stat_bytes() {
  stat -c%s "$1" 2>/dev/null || stat -f%z "$1"
}

bash "$EXP/scripts/build-gtk-deps.sh"   # GLib + cairo/pango/harfbuzz/... rendering core

# 1) gdk-pixbuf (glib + libpng).
if [ ! -f "$PREFIX/lib/libgdk_pixbuf-2.0.a" ]; then
  cd "$TP"; [ -d gdk-pixbuf ] || { curl -fsSL -o gdk-pixbuf.tar.xz "https://download.gnome.org/sources/gdk-pixbuf/2.42/gdk-pixbuf-2.42.12.tar.xz"; tar xf gdk-pixbuf.tar.xz && mv gdk-pixbuf-2.42.12 gdk-pixbuf; }
  cd gdk-pixbuf && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" -Dtests=false -Dman=false \
    -Dintrospection=disabled -Dpng=enabled -Djpeg=disabled -Dtiff=disabled -Dgio_sniffing=false \
    -Dbuiltin_loaders=png -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" gdk-pixbuf/libgdk_pixbuf-2.0.a >/dev/null 2>&1
  cp -f "$BD"/gdk-pixbuf/libgdk_pixbuf-2.0.a "$PREFIX/lib/"
  find "$BD" -name "gdk-pixbuf-2.0.pc" -exec cp -f {} "$PCDIR/" \;
  mkdir -p "$PREFIX/include/gdk-pixbuf-2.0/gdk-pixbuf"; cp -f gdk-pixbuf/*.h "$BD"/gdk-pixbuf/*.h "$PREFIX/include/gdk-pixbuf-2.0/gdk-pixbuf/" 2>/dev/null
fi
echo "gdk-pixbuf: $(PKG_CONFIG_LIBDIR=$PCDIR pkg-config --modversion gdk-pixbuf-2.0 2>/dev/null)"

# 2) atk + a stub atk-bridge (no accessibility bus in the sandbox).
if [ ! -f "$PREFIX/lib/libatk-1.0.a" ]; then
  cd "$TP"; [ -d atk ] || { curl -fsSL -o atk.tar.xz "https://download.gnome.org/sources/atk/2.38/atk-2.38.0.tar.xz"; tar xf atk.tar.xz && mv atk-2.38.0 atk; }
  cd atk && rm -rf "$BD"
  meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" -Dintrospection=false -Ddefault_library=static >/dev/null 2>&1
  ninja -C "$BD" atk/libatk-1.0.a >/dev/null 2>&1
  cp -f "$BD"/atk/libatk-1.0.a "$PREFIX/lib/"; find "$BD" -name "atk.pc" -exec cp -f {} "$PCDIR/" \;
  mkdir -p "$PREFIX/include/atk-1.0/atk"; cp -f atk/*.h "$BD"/atk/*.h "$PREFIX/include/atk-1.0/atk/" 2>/dev/null
fi
if [ ! -f "$PREFIX/lib/libatk-bridge-2.0.a" ]; then
  "$CC" $CFLAGS -I "$EXP/glib-deps/atk-bridge-stub" -c "$EXP/glib-deps/atk-bridge-stub/atk-bridge.c" -o "$EXP/glib-deps/atk-bridge-stub/atk-bridge.o"
  "$AR" rcs "$PREFIX/lib/libatk-bridge-2.0.a" "$EXP/glib-deps/atk-bridge-stub/atk-bridge.o"
  mkdir -p "$PREFIX/include/at-spi2-atk/2.0"; cp -f "$EXP/glib-deps/atk-bridge-stub/atk-bridge.h" "$PREFIX/include/at-spi2-atk/2.0/"
  printf 'prefix=%s\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\nName: atk-bridge-2.0\nDescription: AT-SPI atk bridge (wasm stub)\nVersion: 2.38.0\nRequires: atk\nLibs: -L${libdir} -latk-bridge-2.0\nCflags: -I${includedir}/at-spi2-atk/2.0\n' "$PREFIX" > "$PCDIR/atk-bridge-2.0.pc"
fi
echo "atk: $(PKG_CONFIG_LIBDIR=$PCDIR pkg-config --modversion atk 2>/dev/null) ; atk-bridge stub installed"

# 3) X libs GTK needs (proto .pc stubs first, then the libs via autotools).
for proto in randrproto compositeproto damageproto; do
  [ -f "$PCDIR/$proto.pc" ] || printf 'prefix=%s\nincludedir=${prefix}/include\nName: %s\nDescription: X11 %s headers\nVersion: 2024.1\nCflags: -I${includedir}\n' "$PREFIX" "$proto" "$proto" > "$PCDIR/$proto.pc"
done
xbuild() { # name url dir liba   (threaded: build in a per-profile <dir>${SFX} source copy)
  [ -f "$PREFIX/lib/$4" ] && return
  cd "$TP"; [ -d "$3" ] || { curl -fsSL -o "$3.tar.xz" "$2" && tar xf "$3.tar.xz" && mv "$(basename "$2" .tar.xz)" "$3"; }
  local d="$3$SFX"; [ -d "$TP/$d" ] || cp -r "$TP/$3" "$TP/$d"
  cd "$TP/$d" && make distclean >/dev/null 2>&1 || true
  # NOREGEN: the cp -r copy (threaded) bumps timestamps so the recovered Makefiles try to re-run
  # aclocal/autoconf (absent on this host); neutralize the maintainer-mode regen rules.
  local NR="ACLOCAL=true AUTOCONF=true AUTOMAKE=true AUTOHEADER=true MAKEINFO=true AUTORECONF=true"
  CC="$CC" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS -L$PREFIX/lib" AR="$AR" RANLIB="$WSDK/bin/llvm-ranlib" PKG_CONFIG_LIBDIR="$PCDIR" \
    ./configure --host=wasm32-wasi --prefix="$PREFIX" --enable-static --disable-shared --disable-malloc0returnsnull >/dev/null 2>&1
  eval "make -j4 $NR" >/dev/null 2>&1; eval "make install $NR" >/dev/null 2>&1
}
xbuild Xcursor     "https://www.x.org/releases/individual/lib/libXcursor-1.2.2.tar.xz"     libXcursor    libXcursor.a
xbuild Xrandr      "https://www.x.org/releases/individual/lib/libXrandr-1.5.4.tar.xz"      libXrandr     libXrandr.a
xbuild Xcomposite  "https://www.x.org/releases/individual/lib/libXcomposite-0.4.6.tar.xz" libXcomposite libXcomposite.a
xbuild Xdamage     "https://www.x.org/releases/individual/lib/libXdamage-1.1.6.tar.xz"    libXdamage    libXdamage.a
# Meson function probes do not ask pkg-config for --static libs here, but libXext.a needs
# X11 and the threaded host socket shims for XSync detection. Patch xext.pc only while
# configuring/building GTK; downstream app links add libhostcompat explicitly.
if [ -f "$PCDIR/xext.pc" ]; then
  perl -0pi -e 's/^Libs:.*$/Libs: -L\${libdir} -lXext/m' "$PCDIR/xext.pc"
  XEXT_PC_BAK="$(mktemp "${TMPDIR:-/tmp}/sx-xext-pc.XXXXXX")"
  cp -f "$PCDIR/xext.pc" "$XEXT_PC_BAK"
  trap 'cp -f "$XEXT_PC_BAK" "$PCDIR/xext.pc"; rm -f "$XEXT_PC_BAK"' EXIT
  XEXT_HOST_SOCKET="$EXP/toolchain/threads-libs/host_socket.o" \
    perl -0pi -e 's/^Libs:.*$/"Libs: -L\${libdir} -lXext -lX11 -lxcb -lXau -lXdmcp $ENV{XEXT_HOST_SOCKET}"/me' "$PCDIR/xext.pc"
fi
echo "X libs: $(for l in Xrandr Xcursor Xcomposite Xdamage; do ls $PREFIX/lib/lib$l.a >/dev/null 2>&1 && printf "$l "; done)"

# libepoxy's generated EGL header includes the standard Khronos platform headers. The WASI
# sysroot has no GL/EGL SDK, so stage the small platform typedef surface libepoxy needs.
mkdir -p "$PREFIX/include/EGL" "$PREFIX/include/KHR"
cat > "$PREFIX/include/KHR/khrplatform.h" <<'KHR'
#ifndef __khrplatform_h_
#define __khrplatform_h_
#include <stdint.h>
#include <stddef.h>
typedef int8_t khronos_int8_t;
typedef uint8_t khronos_uint8_t;
typedef int16_t khronos_int16_t;
typedef uint16_t khronos_uint16_t;
typedef int32_t khronos_int32_t;
typedef uint32_t khronos_uint32_t;
typedef int64_t khronos_int64_t;
typedef uint64_t khronos_uint64_t;
typedef intptr_t khronos_intptr_t;
typedef uintptr_t khronos_uintptr_t;
typedef ptrdiff_t khronos_ssize_t;
typedef float khronos_float_t;
typedef khronos_int64_t khronos_utime_nanoseconds_t;
typedef khronos_int64_t khronos_stime_nanoseconds_t;
#endif
KHR
cat > "$PREFIX/include/EGL/eglplatform.h" <<'EGL'
#ifndef __eglplatform_h_
#define __eglplatform_h_
#include <KHR/khrplatform.h>
typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;
typedef khronos_int32_t EGLint;
#endif
EGL

# 4) Host code-gen tools (glib-compile-* native; gdbus-codegen wrapped to OUR GLib 2.78.4 module).
for t in glib-mkenums glib-genmarshal; do s=$(find "$TP/glib/$GBD" -name "$t" -type f|head -1); [ -n "$s" ] && { cp -f "$s" "$PREFIX/bin/$t"; chmod +x "$PREFIX/bin/$t"; }; done
ln -sf "$(which glib-compile-resources)" "$PREFIX/bin/glib-compile-resources"
ln -sf "$(which glib-compile-schemas)" "$PREFIX/bin/glib-compile-schemas"
CFG=$(find "$TP/glib/$GBD" -name config.py -path "*codegen*"|head -1); [ -n "$CFG" ] && cp -f "$CFG" "$TP/glib/gio/gdbus-2.0/codegen/config.py"
rm -f "$PREFIX/bin/gdbus-codegen"
cat > "$PREFIX/bin/gdbus-codegen" <<GD
#!/usr/bin/env bash
exec python3 -c "import sys; sys.path.insert(0, '$TP/glib/gio/gdbus-2.0'); from codegen import codegen_main; codegen_main.codegen_main()" "\$@"
GD
chmod +x "$PREFIX/bin/gdbus-codegen"

# 5) GTK 3.24 (libepoxy pulled as a subproject; X11 backend; no wayland/introspection/demos/tests).
cd "$TP"; [ -d gtk3 ] || { curl -fsSL -o gtk3.tar.xz "https://download.gnome.org/sources/gtk+/3.24/gtk+-3.24.43.tar.xz"; tar xf gtk3.tar.xz && mv gtk+-3.24.43 gtk3; }
cd gtk3 && rm -rf "$BD"
# GTK references throttled_presentation_time from X11 code even when HAVE_XSYNC is disabled.
# Keep the storage available in that build shape instead of relying on XSync support.
perl -0pi -e 's/\n  gint64 current_counter_value;\n\n  \/\* After a _NET_WM_FRAME_DRAWN message, this is the soonest that we think\n   \* frame after will be presented \*\/\n  gint64 throttled_presentation_time;\n#endif\n/\n  gint64 current_counter_value;\n#endif\n\n  \/\* After a _NET_WM_FRAME_DRAWN message, this is the soonest that we think\n   \* frame after will be presented \*\/\n  gint64 throttled_presentation_time;\n/' gdk/x11/gdkwindow-x11.h
meson setup "$BD" --cross-file "$CROSS_INI" --prefix="$PREFIX" \
  -Dx11_backend=true -Dwayland_backend=false -Dintrospection=false -Ddemos=false -Dexamples=false \
  -Dtests=false -Dprint_backends=file -Dcolord=no -Dtracker3=false -Ddefault_library=static
gtk_targets=(gtk/libgtk-3.a gdk/libgdk-3.a)
if ninja -C "$BD" -t targets all | grep -q '^subprojects/libepoxy/src/libepoxy\.a:'; then
  gtk_targets+=(subprojects/libepoxy/src/libepoxy.a)
fi
ninja -C "$BD" "${gtk_targets[@]}"
# Install the toolkit + .pc into $PREFIX so build-gtk-app.sh's pkg-config resolves gtk+-3.0.
cp -f "$BD"/gtk/libgtk-3.a "$BD"/gdk/libgdk-3.a "$PREFIX/lib/"
[ ! -f "$BD"/subprojects/libepoxy/src/libepoxy.a ] || cp -f "$BD"/subprojects/libepoxy/src/libepoxy.a "$PREFIX/lib/"
[ -f "$PREFIX/lib/libepoxy.a" ] || { echo "ERROR: libepoxy.a missing after GTK build"; exit 1; }
for pc in gtk+-3.0.pc gdk-3.0.pc gtk+-x11-3.0.pc gdk-x11-3.0.pc; do [ -f "$BD/$pc" ] && cp -f "$BD/$pc" "$PCDIR/"; done
[ ! -f "$BD"/meson-private/epoxy.pc ] || cp -f "$BD"/meson-private/epoxy.pc "$PCDIR/"
[ -f "$PCDIR/epoxy.pc" ] || { echo "ERROR: epoxy.pc missing after GTK build"; exit 1; }
mkdir -p "$PREFIX/include/gtk-3.0/gtk" "$PREFIX/include/gtk-3.0/gdk" "$PREFIX/include/gtk-3.0/gdk/x11"
cp -f gtk/*.h "$BD"/gtk/*.h "$PREFIX/include/gtk-3.0/gtk/" 2>/dev/null
cp -f gdk/*.h "$BD"/gdk/*.h "$PREFIX/include/gtk-3.0/gdk/" 2>/dev/null
cp -f gdk/x11/*.h "$PREFIX/include/gtk-3.0/gdk/x11/" 2>/dev/null
cp -rf subprojects/libepoxy/include/epoxy "$BD"/subprojects/libepoxy/include/epoxy "$PREFIX/include/" 2>/dev/null
echo "== M8: GTK 3.24 cross-compiles to wasm32-wasip1${SECURE_EXEC_WASM_THREADS:+-threads} -> $(stat_bytes "$BD"/gtk/libgtk-3.a) bytes (libgtk-3 + libgdk-3 + epoxy installed) =="
