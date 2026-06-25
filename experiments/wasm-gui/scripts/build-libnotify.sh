#!/usr/bin/env bash
# XU6 dep: libnotify 0.8.3 (xfce4-notifyd's client lib + the notification API) to wasm32-wasip1-threads.
# meson cross, static. Deps gdk-pixbuf/glib/gio all built. Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1; source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"; PREFIX="$TP/wasm-prefix-threads"; PCDIR="$PREFIX/lib/pkgconfig"; BD="build-wasm-threads"
SRC="$TP/libnotify"; [ -d "$SRC" ] || { echo "FATAL: libnotify not fetched"; exit 1; }
cd "$SRC"; rm -rf "$BD"
echo "== meson setup libnotify =="
meson setup "$BD" --cross-file "$CROSS_INI" --wrap-mode=nofallback --prefix="$PREFIX" \
  -Dtests=false -Dintrospection=disabled -Dman=false -Dgtk_doc=false -Ddocbook_docs=disabled \
  -Ddefault_library=static > /tmp/libnotify-setup.log 2>&1
RC=$?; if [ $RC -ne 0 ]; then echo "SETUP FAILED:"; tail -30 /tmp/libnotify-setup.log; exit 1; fi
echo "meson setup OK"
# libnotify forces shared_library(); the .so link fails for wasm but the objects compile -> archive them.
ninja -C "$BD" 2>> /tmp/libnotify-ninja.log || true
APDIR="$(find "$BD/libnotify" -type d -name 'libnotify.so*.p' | head -1)"
if [ -n "$APDIR" ]; then
  rm -f "$PREFIX/lib/libnotify.a"
  "$WSDK/bin/llvm-ar" crs "$PREFIX/lib/libnotify.a" $(find "$APDIR" -name '*.o' 2>/dev/null)
  PC="$(find "$BD" -name 'libnotify.pc' | head -1)"; [ -n "$PC" ] && cp -f "$PC" "$PCDIR/"
  mkdir -p "$PREFIX/include/libnotify"
  cp -f "$SRC"/libnotify/*.h "$PREFIX/include/libnotify/" 2>/dev/null
  find "$BD/libnotify" -name '*.h' -exec cp -f {} "$PREFIX/include/libnotify/" \; 2>/dev/null
  echo "built libnotify.a ($(stat -c%s "$PREFIX/lib/libnotify.a")); pc=$([ -n "$PC" ] && echo yes)"
else echo "BUILD FAILED:"; tail -20 /tmp/libnotify-ninja.log; fi
