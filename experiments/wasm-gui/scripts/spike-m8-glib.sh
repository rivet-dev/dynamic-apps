#!/usr/bin/env bash
# M8 spike: attempt to cross-compile GLib (the GTK stack's foundation) to wasm32-wasip1 with the same
# wasi-sdk/meson toolchain used for the X stack. Documents how far the GTK-family port gets. See
# M8-FINDINGS.md: this currently fails at the libffi subproject (no wasm/wasi FFI trampolines), which
# GObject (and therefore all of GTK) requires.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"
GLIB_VER="2.78.4"

mkdir -p "$TP"
if [ ! -d "$TP/glib" ]; then
  echo "== fetching GLib $GLIB_VER =="
  curl -fsSL -o "$TP/glib.tar.xz" \
    "https://download.gnome.org/sources/glib/2.78/glib-${GLIB_VER}.tar.xz" || {
      echo "download failed"; exit 1; }
  ( cd "$TP" && tar xf glib.tar.xz && mv "glib-${GLIB_VER}" glib )
fi

cd "$TP/glib"
rm -rf build-wasm
echo "== meson setup (wasm32-wasi cross) =="
meson setup build-wasm --cross-file "$CROSS_INI" \
  -Dtests=false -Dglib_debug=disabled -Dnls=disabled -Dlibmount=disabled -Dselinux=disabled \
  -Ddtrace=false -Dsystemtap=false -Ddefault_library=static 2>&1 | tail -25
echo "== (expected to fail at the libffi subproject: no wasm/wasi FFI trampolines — see M8-FINDINGS.md) =="
