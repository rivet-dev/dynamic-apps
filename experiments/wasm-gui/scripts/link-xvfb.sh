#!/usr/bin/env bash
# Rebuild the X server objects (ninja) then link Xvfb.wasm with the fixups wasm-ld/secure-exec need:
#   - force __main_argc_argv (wasi crt only weak-refs main, so it gets GC'd)
#   - append libfontenc/freetype/z/Xau/Xdmcp + libsetjmp + the patched libc (archive ordering)
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
BW="$EXP/third_party/xserver/build-wasm"
TOOLCHAIN_HOME="${SECURE_EXEC_TOOLCHAIN_HOME:-/home/nathan/secure-exec}"
WSDK="$TOOLCHAIN_HOME/registry/native/c/vendor/wasi-sdk"
SETJMP="$WSDK/share/wasi-sysroot/lib/wasm32-wasip1/libsetjmp.a"
LIBC="$TOOLCHAIN_HOME/registry/native/c/sysroot/lib/wasm32-wasip1/libc.a"
P="$EXP/third_party/wasm-prefix/lib"
MAINOBJ="$EXP/toolchain/xvfb-main.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

# rebuild objects (the default final link fails on weak main — that's expected/ignored)
# -k 0: keep compiling all objects even if some unrelated target (e.g. an xcb-using test client)
# fails to link; we only need the dix/os/hw/vfb objects for the custom Xvfb link below.
ninja -k 0 -C "$BW" >/dev/null 2>&1 || true

ninja -C "$BW" -t commands hw/vfb/Xvfb 2>/dev/null | tail -1 > /tmp/link-xvfb.sh
sed -i "s| -o hw/vfb/Xvfb| -Wl,--undefined=__main_argc_argv $MAINOBJ -o hw/vfb/Xvfb|" /tmp/link-xvfb.sh
sed -i "s| -Wl,--end-group| $P/libXfont2.a $P/libfontenc.a $P/libfreetype.a $P/libz.a $P/libXau.a $P/libXdmcp.a $SETJMP $LIBC -Wl,--end-group|" /tmp/link-xvfb.sh
( cd "$BW" && rm -f hw/vfb/Xvfb && bash /tmp/link-xvfb.sh )
RC=$?
if [ -f "$BW/hw/vfb/Xvfb" ]; then
  # fpcast-emu: the dix device-class init (InitPtrFeedbackClassDeviceStruct et al.) calls fn-ptrs
  # across signatures; without emulation wasm traps "null function or function signature mismatch".
  # --strip-debug/--strip-dwarf: the linked X/font/glib libs carry DWARF that bloats the module and
  # OOMs the V8 isolate. Mirrors the openbox/lxpanel guest post-process.
  wasm-opt --fpcast-emu --pass-arg=max-func-params@128 --strip-debug --strip-dwarf \
    --enable-bulk-memory --enable-threads -O0 "$BW/hw/vfb/Xvfb" -o "$EXP/Xvfb.wasm" 2>/dev/null \
    || { echo "wasm-opt FAILED"; exit 1; }
  echo "linked + fpcast Xvfb.wasm ($(stat -c%s "$EXP/Xvfb.wasm") bytes)"
else
  echo "LINK FAILED (rc=$RC)"; exit 1
fi
