#!/usr/bin/env bash
# Build + install the libffi-wasm shim as an external dependency the cross meson build can find:
# a static archive (libffi.a), headers (ffi.h/ffitarget.h), and a pkg-config file (libffi.pc) installed
# into third_party/wasm-prefix (the cross file's pkg_config_libdir). This lets GLib's meson resolve
# `dependency('libffi')` against this shim instead of the unbuildable libffi subproject — surfacing the
# NEXT real GLib/GObject wasi blocker (threads).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
PREFIX="$EXP/third_party/wasm-prefix"
INC="$EXP/libffi-wasm/include"
mkdir -p "$PREFIX/lib/pkgconfig" "$PREFIX/include"

echo "== compile libffi-wasm/src/ffi.c =="
"$CC" $CFLAGS -I"$INC" -c "$EXP/libffi-wasm/src/ffi.c" -o "$EXP/libffi-wasm/ffi.o" 2>&1 | grep -iE "error" | head
"$AR" rcs "$PREFIX/lib/libffi.a" "$EXP/libffi-wasm/ffi.o"
cp -f "$INC/ffi.h" "$INC/ffitarget.h" "$PREFIX/include/"

cat > "$PREFIX/lib/pkgconfig/libffi.pc" <<EOF
prefix=$PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: libffi
Description: libffi-wasm shim (secure-exec wasm32-wasip1)
Version: 3.4.4
Libs: -L\${libdir} -lffi
Cflags: -I\${includedir}
EOF

echo "installed: $PREFIX/lib/libffi.a + libffi.pc + ffi.h/ffitarget.h"
PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" pkg-config --modversion libffi && echo "pkg-config sees libffi OK"
