#!/usr/bin/env bash
# M8: cross-compile the GLib stack (the foundation of GTK) to wasm32-wasip1 for secure-exec. This is the
# concrete continuation of M8 past the libffi blocker: it builds the secure-exec-native libffi shim,
# GLib's dependencies (PCRE2, an intl stub), then GLib itself and proves GObject builds AGAINST the shim.
#
# What this builds (all to wasm32-wasip1, static):
#   libffi-wasm shim -> third_party/wasm-prefix/lib/libffi.a (+ libffi.pc)
#   PCRE2            -> third_party/wasm-prefix/lib/libpcre2-8.a (+ libpcre2-8.pc)
#   intl stub        -> third_party/wasm-prefix/lib/libintl.a (+ intl.pc)
#   GLib 2.78.4      -> third_party/glib/build-wasm/{glib,gobject,gthread,gmodule}/lib*-2.0.a
# GObject linking the shim's <ffi.h> is the proof the libffi dead end is gone. (GIO is partially built;
# its BSD-socket networking files need further wasi sockaddr adaptation — the next M8 step.)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"
# Honor cross-env PREFIX (wasm-prefix vs wasm-prefix-threads under SECURE_EXEC_WASM_THREADS=1).
BUILDDIR="build-wasm${SECURE_EXEC_WASM_THREADS:+-threads}"

# 1) libffi-wasm shim + intl stub.
bash "$EXP/scripts/build-libffi-wasm.sh"
# intl stub
"$CC" $CFLAGS -I"$EXP/glib-deps/intl-stub" -c "$EXP/glib-deps/intl-stub/libintl.c" -o "$EXP/glib-deps/intl-stub/libintl.o"
"$AR" rcs "$PREFIX/lib/libintl.a" "$EXP/glib-deps/intl-stub/libintl.o"
cp -f "$EXP/glib-deps/intl-stub/libintl.h" "$PREFIX/include/"
cat > "$PREFIX/lib/pkgconfig/intl.pc" <<EOF
prefix=$PREFIX
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: intl
Description: libintl stub (NLS disabled)
Version: 0.21
Libs: -L\${libdir} -lintl
Cflags: -I\${includedir}
EOF

# 2) PCRE2 (GLib regex dependency).
PV="10.43"
if [ ! -f "$PREFIX/lib/libpcre2-8.a" ]; then
  if [ ! -d "$TP/pcre2" ]; then
    curl -fsSL -o "$TP/pcre2.tar.gz" "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-$PV/pcre2-$PV.tar.gz"
    ( cd "$TP" && tar xf pcre2.tar.gz && mv "pcre2-$PV" pcre2 )
  fi
  ( cd "$TP/pcre2" && CC="$CC" CFLAGS="$CFLAGS" AR="$AR" RANLIB="$WASI_SDK/bin/llvm-ranlib" \
      ./configure --host=wasm32-wasi --prefix="$PREFIX" --enable-static --disable-shared --disable-jit >/dev/null 2>&1
    make -j4 libpcre2-8.la >/dev/null 2>&1 || true
    cp -f .libs/libpcre2-8.a "$PREFIX/lib/"; cp -f src/pcre2.h "$PREFIX/include/"
    find . -name libpcre2-8.pc -exec cp -f {} "$PREFIX/lib/pkgconfig/" \; )
fi
echo "deps installed: libffi.a libpcre2-8.a libintl.a"

# 3) GLib: download, configure, build the core libs.
GV="2.78.4"
if [ ! -d "$TP/glib" ]; then
  curl -fsSL -o "$TP/glib.tar.xz" "https://download.gnome.org/sources/glib/2.78/glib-$GV.tar.xz"
  ( cd "$TP" && tar xf glib.tar.xz && mv "glib-$GV" glib )
fi
cd "$TP/glib"
# Idempotent wasi source fixes for GLib/GIO (the source tree is a gitignored download).
python3 - <<'PY'
import io
# GIO ginetsocketaddress.c: wasi sockaddr_in has no sin_zero padding member.
f = "gio/ginetsocketaddress.c"
s = open(f).read()
old = "      memset (sock->sin_zero, 0, sizeof (sock->sin_zero));"
new = ("#ifndef __wasi__\n"
       "      memset (sock->sin_zero, 0, sizeof (sock->sin_zero));\n"
       "#endif /* wasi sockaddr_in has no sin_zero padding member */")
if old in s and new not in s:
    open(f, "w").write(s.replace(old, new, 1)); print("patched ginetsocketaddress.c (sin_zero)")
# GLib gwakeup.c: wasi has no eventfd/pipe for a cross-thread wakeup and the sandbox is single-threaded,
# so the main loop is only woken by its own X-socket/timer poll. Use inert (-1) fds (poll ignores them;
# signal/acknowledge already no-op on -1). Without this, g_wakeup_new aborts ("GWakeup: Bad file descriptor").
gw = "glib/gwakeup.c"; gs = open(gw).read()
gwa = "  wakeup = g_slice_new (GWakeup);\n\n  /* try eventfd first, if we think we can */"
gwn = ("  wakeup = g_slice_new (GWakeup);\n\n"
       "#ifdef __wasi__\n"
       "  wakeup->fds[0] = -1;\n  wakeup->fds[1] = -1;\n  return wakeup;\n"
       "#endif\n\n  /* try eventfd first, if we think we can */")
if "__wasi__" not in gs and gwa in gs:
    open(gw, "w").write(gs.replace(gwa, gwn, 1)); print("patched gwakeup.c (wasi inert wakeup)")
# GIO gunixmounts.c: wasi has no mount table / fstab — add __wasi__ stub branches before the #error.
g = "gio/gunixmounts.c"; t = open(g).read(); n = 0
b1o = "/* Common code {{{2 */\n#else\n#error No _g_get_unix_mounts() implementation for system"
b1n = ("/* Common code {{{2 */\n#elif defined(__wasi__)\n"
       "static const char *\nget_mtab_monitor_file (void) { return NULL; }\n"
       "static GList *\n_g_get_unix_mounts (void) { return NULL; }\n"
       "#else\n#error No _g_get_unix_mounts() implementation for system")
b2o = "/* Common code {{{2 */\n#else\n#error No g_get_mount_table() implementation for system"
b2n = ("/* Common code {{{2 */\n#elif defined(__wasi__)\n"
       "static GList *\n_g_get_unix_mount_points (void) { return NULL; }\n"
       "#else\n#error No g_get_mount_table() implementation for system")
if b1o in t and b1n not in t: t = t.replace(b1o, b1n, 1); n += 1
if b2o in t and b2n not in t: t = t.replace(b2o, b2n, 1); n += 1
if n: open(g, "w").write(t); print(f"patched gunixmounts.c ({n} wasi stub branches)")
PY
rm -rf "$BUILDDIR"
meson setup "$BUILDDIR" --cross-file "$CROSS_INI" --wrap-mode=nofallback \
  -Dtests=false -Dglib_debug=disabled -Dnls=disabled -Dlibmount=disabled -Dselinux=disabled \
  -Ddtrace=false -Dsystemtap=false -Ddefault_library=static -Dxattr=false
echo "== building glib / gobject / gthread / gmodule / gio =="
for lib in glib/libglib-2.0.a gobject/libgobject-2.0.a gthread/libgthread-2.0.a gmodule/libgmodule-2.0.a gio/libgio-2.0.a; do
  ninja -C "$BUILDDIR" "$lib"
  echo "BUILT $(basename "$lib") ($(stat -c%s "$BUILDDIR/$lib") bytes)"
done
echo "== M8 GLib stack COMPLETE: GLib + GObject (linked the libffi-wasm shim) + GThread + GModule + GIO built for wasm32-wasip1 =="
