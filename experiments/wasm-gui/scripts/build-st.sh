#!/usr/bin/env bash
# Build the suckless terminal emulator `st` to wasm32-wasip1 against the cross-compiled X11 + Xft +
# fontconfig + freetype stack, with st's forkpty/select/read/write PTY backend replaced by the
# secure-exec kernel-PTY primitive (wasmpty.c -> host_net.pty_spawn/pty_read/pty_write). The result is
# a real terminal emulator running entirely as a wasm guest in secure-exec, driving a wasm shell over a
# kernel PTY and rendering the terminal grid via Xft to the wasm X server. See SPEC.md M6.3.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
SRC="$EXP/third_party/st"
P="$EXP/third_party/wasm-prefix"
SETJMP="$WSDK/share/wasi-sysroot/lib/wasm32-wasip1/libsetjmp.a"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
COMPAT="$EXP/toolchain/wasi-compat.o"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
# The patched st source tree is tracked under third_party/st. If it is absent (fresh checkout / wiped),
# fetch upstream st-0.9.2 so the build is reproducible. NOTE: the secure-exec PTY patches (wasmpty.c +
# st.c/x.c edits) live in the tracked tree; a re-fetch yields vanilla st without them.
if [ ! -d "$SRC" ]; then
  echo "third_party/st missing; fetching upstream st-0.9.2 (vanilla; re-apply patches if needed)"
  curl -fsSL -o "$EXP/third_party/st.tar.gz" "https://dl.suckless.org/st/st-0.9.2.tar.gz" \
    && ( cd "$EXP/third_party" && tar xf st.tar.gz && mv st-0.9.2 st ) \
    || { echo "fetch failed"; exit 1; }
fi

cd "$SRC"
# config.h: monospace Xft font we ship via fontconfig (prepare-xftfonts.sh), and our wasm shell guest.
if [ ! -f config.h ] || [ config.def.h -nt config.h ]; then
  sed -e 's#"Liberation Mono:pixelsize=12:antialias=true:autohint=true"#"DejaVu Sans Mono:pixelsize=14:antialias=true:autohint=true"#' \
      config.def.h > config.h
fi

INCS="-I$P/include -I$P/include/freetype2 -DVERSION=\"0.9.2\" -D_XOPEN_SOURCE=600 -DSTWASM_SHELL=\"/pty-shell.wasm\""
OUT="$EXP/st.wasm"
rm -f st.o x.o wasmpty.o "$OUT"

for tu in st x wasmpty; do
  # shellcheck disable=SC2086
  "$CC" $CFLAGS $INCS -c -o "$tu.o" "$tu.c" 2>&1 | grep -iE "error" | head -20
  [ -f "$tu.o" ] || { echo "COMPILE FAILED ($tu.c)"; exit 1; }
done

# shellcheck disable=SC2086
"$CC" $CFLAGS $LDFLAGS -Wl,--allow-undefined \
  -o "$OUT" st.o x.o wasmpty.o "$COMPAT" \
  -L"$P/lib" -lXft -lfontconfig -lfreetype -lexpat -lz -lXrender -lXext -lXmu -lX11 -lSM -lICE -lxcb -lXau -lXdmcp \
  -lwasi-emulated-mman -lwasi-emulated-process-clocks \
  "$SETJMP" "$LIBC" 2>&1 | grep -iE "error|undefined" | head -30
[ -f "$OUT" ] || { echo "LINK FAILED (st)"; exit 1; }

wasm-opt --fpcast-emu -O0 "$OUT" -o "$OUT.fp" 2>/dev/null && mv "$OUT.fp" "$OUT"
echo "built st.wasm ($(stat -c%s "$OUT") bytes)"
