#!/usr/bin/env bash
# Build the M6.3 PTY test guests (guest-xclient/pty-term.c, pty-shell.c) to wasm. These are tiny
# (libc only); pty-term references the host_net.pty_spawn/pty_read/pty_write imports as undefined
# symbols (--allow-undefined turns them into wasm imports), so apply --fpcast-emu like other guests.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
source "$EXP/toolchain/cross-env.sh"
LIBC="$SYSROOT/lib/wasm32-wasip1/libc.a"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

for NAME in pty-shell pty-term; do
  SRC="$EXP/guest-xclient/$NAME.c"
  OUT="$EXP/guest-xclient/$NAME.wasm"
  "$CC" $CFLAGS $LDFLAGS -Wl,--allow-undefined -o "$OUT" "$SRC" "$LIBC" \
    2>&1 | grep -iE "error|undefined" | head -20
  if [ -f "$OUT" ]; then
    wasm-opt --fpcast-emu -O0 "$OUT" -o "$OUT.fp" 2>/dev/null && mv "$OUT.fp" "$OUT"
    echo "built guest-xclient/$NAME.wasm ($(stat -c%s "$OUT") bytes)"
  else
    echo "BUILD FAILED ($NAME)"; exit 1
  fi
done
