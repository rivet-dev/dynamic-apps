#!/usr/bin/env bash
# XU3 diagnostic: build the garcon menu-load probe. Built WITHOUT -g and WITHOUT --strip-debug so the
# wasm NAME section survives (the panel build emits DWARF but no name section -> numeric stack frames).
# This small binary lets the SECURE_EXEC_KEEP_NAMES stackdump symbolize the menu-load deadlock.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
GB="$EXP/third_party/glib"; BD="$GB/build-wasm-threads"
[ -f "$BD/gio/libgio-2.0.a" ] || { echo "FATAL: threaded GIO not built"; exit 1; }
[ -f "$PREFIX/lib/libgarcon-1.a" ] || { echo "FATAL: libgarcon-1.a missing"; exit 1; }
INC="-I$GB -I$GB/glib -I$GB/gmodule -I$GB/gobject -I$GB/gio -I$BD -I$BD/glib -I$BD/gio -I$PREFIX/include/garcon-1"
OUT="$EXP/garcon-probe.wasm"
rm -f "$OUT" "$OUT.elf"
[ -f "$PREFIX/lib/libhostcompat.a" ] || bash "$EXP/scripts/build-libfm.sh" >/dev/null 2>&1 || true
# -g0 (appended; last -g wins) forces lld to emit the NAME section instead of only DWARF.
CFLAGS_NOG="$CFLAGS -g0"
"$CC" $CFLAGS_NOG $INC -Wl,--allow-undefined -Wl,--no-check-features \
  -Wl,--wrap=read -Wl,--wrap=getsockopt \
  -o "$OUT.elf" "$EXP/guest-xclient/garcon-probe.c" \
  -L"$PREFIX/lib" -lgarcon-1 -lxfce4util \
  "$BD/gio/libgio-2.0.a" "$BD/gobject/libgobject-2.0.a" "$BD/gmodule/libgmodule-2.0.a" \
  "$BD/gthread/libgthread-2.0.a" "$BD/glib/libglib-2.0.a" \
  -ldbuscreds -lhostcompat \
  "$PREFIX/lib/libpcre2-8.a" "$PREFIX/lib/libz.a" "$PREFIX/lib/libintl.a" "$PREFIX/lib/libffi.a" \
  $LDFLAGS 2>&1 | grep -iE "error|undefined symbol" | grep -viE "wasm-ld: warning" | head -30
[ -f "$OUT.elf" ] || { echo "LINK FAILED"; exit 1; }
echo "linked $OUT.elf ($(stat -c%s "$OUT.elf") bytes)"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
# keep the name section: fpcast + -Oz --strip-dwarf (NOT --strip-debug, which drops names)
wasm-opt --fpcast-emu --pass-arg=max-func-params@128 --enable-bulk-memory --enable-threads -O0 "$OUT.elf" -o "$OUT.1" 2>/dev/null
wasm-opt -Oz --strip-dwarf --enable-bulk-memory --enable-threads "$OUT.1" -o "$OUT" 2>/dev/null
rm -f "$OUT.1"
[ -f "$OUT" ] && echo "built garcon-probe.wasm ($(stat -c%s "$OUT") bytes)" || { echo "FPCAST FAILED"; exit 1; }
echo "name-section symbols present: $(wasm-dis "$OUT" 2>/dev/null | grep -aoE '\(func \$[a-zA-Z_][a-zA-Z0-9_]*' | grep -avE '\$[0-9]' | head -3 | tr '\n' ' ')"