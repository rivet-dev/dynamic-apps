#!/usr/bin/env bash
# XU6 dep: cross-compile VTE 0.70.6 (xfce4-terminal's terminal widget) to wasm32-wasip1-threads, GTK3,
# WITHOUT icu/gnutls (huge deps -> -Dicu=false -Dgnutls=false; fribidi kept, it is already built). meson
# cross, static. Constraint #5: upstream untouched.
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1; source "$EXP/toolchain/cross-env.sh"
# VTE is C++ and REQUIRES exceptions (try/throw). Inject -fwasm-exceptions into the cross-file's cpp_args
# (append, not override, so the compat-include -I + -matomics etc. are kept). Platform layer, not VTE.
sed -i "s/'-fno-exceptions', //g; s/^\(cpp_args = \[.*\)\]/\1, '-fwasm-exceptions']/" "$CROSS_INI"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
TP="$EXP/third_party"; PREFIX="$TP/wasm-prefix-threads"; PCDIR="$PREFIX/lib/pkgconfig"; BD="build-wasm-threads"
SRC="$TP/vte"; [ -d "$SRC" ] || { echo "FATAL: vte not fetched"; exit 1; }
cd "$SRC"; rm -rf "$BD"
echo "== meson setup vte (no icu/gnutls, gtk3) =="
meson setup "$BD" --cross-file "$CROSS_INI" --wrap-mode=nofallback --prefix="$PREFIX" \
  -Dgnutls=false -Dicu=false -Dgtk4=false -Dgtk3=true -Dfribidi=true -Dvapi=false -Dgir=false \
  -Da11y=false -Ddocs=false -Ddefault_library=static > /tmp/vte-setup.log 2>&1
RC=$?; if [ $RC -ne 0 ]; then echo "SETUP FAILED:"; tail -35 /tmp/vte-setup.log; exit 1; fi
echo "meson setup OK"
echo "== ninja the vte static lib =="
LIBTGT="$(ninja -C "$BD" -t targets all 2>/dev/null | grep -aoE 'src/libvte-2\.91[a-z]*\.a' | sort -u | head -1)"
echo "lib target: $LIBTGT"
ninja -C "$BD" "$LIBTGT" > /tmp/vte-ninja.log 2>&1
RC=$?; echo "ninja rc=$RC"
LIBA="$BD/$LIBTGT"
if [ -f "$LIBA" ]; then
  # FAT archive (embed the .o; meson thin archives do not resolve downstream).
  rm -f "$PREFIX/lib/libvte-2.91.a"
  "$WSDK/bin/llvm-ar" crs "$PREFIX/lib/libvte-2.91.a" $(find "$BD/src/${LIBTGT##*/}.p" -name '*.o' 2>/dev/null)
  PC="$(find "$BD" -name 'vte-2.91.pc' | head -1)"; [ -n "$PC" ] && cp -f "$PC" "$PCDIR/"
  mkdir -p "$PREFIX/include/vte-2.91/vte"
  ( cd "$SRC" && find src -name '*.h' | tar -cf - -T - ) | tar -xf - -C /tmp 2>/dev/null
  cp -f "$SRC"/src/vte/*.h "$PREFIX/include/vte-2.91/vte/" 2>/dev/null
  find "$BD/src" -name '*.h' -exec cp -f {} "$PREFIX/include/vte-2.91/vte/" \; 2>/dev/null
  echo "built libvte-2.91.a ($(stat -c%s "$PREFIX/lib/libvte-2.91.a")); pc=$([ -n "$PC" ] && echo yes)"
else echo "BUILD FAILED; ninja tail:"; tail -25 /tmp/vte-ninja.log; fi
