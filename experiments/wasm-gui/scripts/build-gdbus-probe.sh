#!/usr/bin/env bash
# XU1 linchpin: build the GDBus client probe (guest-xclient/gdbus-probe.c) against the THREADED
# GLib/GIO stack + the dbus_creds host_net shims, so GIO's GDBus can reach the wasm dbus-daemon.
# Proves GDBus-over-host_net before building xfconf/xfsettingsd on top of it (constraint #4).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
export SECURE_EXEC_WASM_THREADS=1
source "$EXP/toolchain/cross-env.sh"
GB="$EXP/third_party/glib"; BD="$GB/build-wasm-threads"
[ -f "$BD/gio/libgio-2.0.a" ] || { echo "FATAL: threaded GIO not built"; exit 1; }
[ -f "$PREFIX/lib/libdbuscreds.a" ] || { echo "FATAL: libdbuscreds.a missing (run build-dbus.sh)"; exit 1; }
INC="-I$GB -I$GB/glib -I$GB/gmodule -I$GB/gobject -I$GB/gio -I$BD -I$BD/glib -I$BD/gio"
OUT="$EXP/gdbus-probe.wasm"
rm -f "$OUT"
# libhostcompat.a already bundles wasi-compat-threads.o + host_socket + the libc overrides; rebuild it
# if a concurrent session churned it away (it is a gitignored artifact).
[ -f "$PREFIX/lib/libhostcompat.a" ] || bash "$EXP/scripts/build-libfm.sh" >/dev/null 2>&1 || true
# dbus_creds provides host_net recvmsg/sendmsg/read(--wrap)/getsockopt(SO_PEERCRED)(--wrap) so GIO's
# GSocket/GCredentials EXTERNAL-auth path works over host_net AF_UNIX — same shims as the dbus binaries.
"$CC" $CFLAGS $INC -Wl,--allow-undefined -Wl,--no-check-features \
  -Wl,--wrap=read -Wl,--wrap=getsockopt \
  -o "$OUT.elf" "$EXP/guest-xclient/gdbus-probe.c" \
  "$BD/gio/libgio-2.0.a" "$BD/gobject/libgobject-2.0.a" "$BD/gmodule/libgmodule-2.0.a" \
  "$BD/gthread/libgthread-2.0.a" "$BD/glib/libglib-2.0.a" \
  -L"$PREFIX/lib" -ldbuscreds -lhostcompat \
  "$PREFIX/lib/libpcre2-8.a" "$PREFIX/lib/libz.a" "$PREFIX/lib/libintl.a" "$PREFIX/lib/libffi.a" \
  $LDFLAGS 2>&1 | grep -iE "error|undefined symbol" | grep -viE "wasm-ld: warning" | head -30
[ -f "$OUT.elf" ] || { echo "LINK FAILED"; exit 1; }
echo "linked $OUT.elf ($(stat -c%s "$OUT.elf") bytes); fpcast..."
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
wasm-opt --fpcast-emu --pass-arg=max-func-params@128 --strip-debug --strip-dwarf \
  --enable-bulk-memory --enable-threads -O0 "$OUT.elf" -o "$OUT" 2>/dev/null
[ -f "$OUT" ] && echo "built gdbus-probe.wasm ($(stat -c%s "$OUT") bytes)" || { echo "FPCAST FAILED"; exit 1; }
