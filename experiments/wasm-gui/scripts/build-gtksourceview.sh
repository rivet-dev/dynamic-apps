#!/usr/bin/env bash
# XU6 dep: cross-compile gtksourceview-4.8.4 (mousepad's source-view widget dep) to wasm32-wasip1-threads.
# meson, on top of the already-built gtk3 + libxml2 + glib. Static. Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"; PREFIX="$TP/wasm-prefix-threads"; PCDIR="$PREFIX/lib/pkgconfig"
BD="build-wasm-threads"
SRC="$TP/gtksourceview"
[ -d "$SRC" ] || { echo "FATAL: gtksourceview not fetched"; exit 1; }
cd "$SRC"; rm -rf "$BD"
echo "== meson setup gtksourceview =="
meson setup "$BD" --cross-file "$CROSS_INI" --wrap-mode=nofallback --prefix="$PREFIX" \
  -Dgir=false -Dvapi=false -Dgtk_doc=false -Dglade_catalog=false -Dinstall_tests=false \
  -Ddefault_library=static > /tmp/gsv-setup.log 2>&1
RC=$?
if [ $RC -ne 0 ]; then echo "SETUP FAILED; tail:"; tail -30 /tmp/gsv-setup.log; exit 1; fi
echo "meson setup OK"
echo "== ninja libgtksourceview-4core.a (the static core; the .so just link_whole's it) =="
ninja -C "$BD" gtksourceview/libgtksourceview-4core.a > /tmp/gsv-ninja.log 2>&1
RC=$?
echo "ninja rc=$RC"
if [ -f "$BD/gtksourceview/libgtksourceview-4core.a" ]; then
  # FAT archive (embed the .o): meson emits a THIN libgtksourceview-4core.a whose relative .o paths do
  # not resolve from a downstream app's build dir ("could not get the buffer for a child of the archive").
  rm -f "$PREFIX/lib/libgtksourceview-4.a"
  "$WSDK/bin/llvm-ar" crs "$PREFIX/lib/libgtksourceview-4.a" $(find "$BD/gtksourceview/libgtksourceview-4core.a.p" -name '*.o')
  PC="$(find "$BD" -name 'gtksourceview-4.pc' | head -1)"; [ -n "$PC" ] && cp -f "$PC" "$PCDIR/"
  mkdir -p "$PREFIX/include/gtksourceview-4"
  # Recursive headers: the umbrella gtksource.h includes subdir headers (completion-providers/words/...).
  ( cd "$SRC" && find gtksourceview -name '*.h' | tar -cf - -T - ) | tar -xf - -C "$PREFIX/include/gtksourceview-4/"
  ( cd "$BD" && find gtksourceview -name '*.h' 2>/dev/null | tar -cf - -T - ) | tar -xf - -C "$PREFIX/include/gtksourceview-4/" 2>/dev/null
  echo "built libgtksourceview-4.a ($(stat -c%s "$PREFIX/lib/libgtksourceview-4.a")); pc=$([ -n "$PC" ] && echo yes)"
else echo "BUILD FAILED; ninja tail:"; tail -25 /tmp/gsv-ninja.log; fi
