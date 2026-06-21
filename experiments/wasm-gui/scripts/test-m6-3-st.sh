#!/usr/bin/env bash
# M6.3 terminal emulator test: the suckless terminal `st`, cross-compiled to wasm32-wasip1 from source
# (scripts/build-st.sh), runs as a wasm guest in secure-exec. It spawns the wasm shell
# (/pty-shell.wasm) over a real kernel PTY via the host_net.pty_spawn primitive (st's forkpty/select
# PTY backend replaced by wasmpty.c), and renders the shell's terminal output via Xft to the wasm X
# server (Xvfb). Asserts the captured framebuffer shows the shell's prompt rendered as antialiased text
# (proving: st instantiated, spawned + read the PTY child, and drew its output through the X stack).
#
# Interactive stdin (terminal -> shell) is proven separately and deterministically by
# scripts/test-m6-3-pty.sh (echo/ping/exit round-trips); this test proves the real terminal-emulator
# rendering path end to end.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XFTFONTS="${VMXFT:-/tmp/vmxft}"; VMLOCALE="${VMLOCALE:-/tmp/vmlocale}"
FB="$(mktemp /tmp/m6st-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/st.wasm" "$EXP/guest-xclient/pty-shell.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f (build: cargo build -p wasm-gui-host -p secure-exec-sidecar; scripts/build-st.sh; scripts/build-pty-guests.sh)"; exit 1; }
done
[ -d "$XFTFONTS" ] || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFTFONTS" >/dev/null 2>&1
[ -d "$VMLOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$VMLOCALE" >/dev/null 2>&1

echo "== Xvfb + st (wasm terminal emulator) spawning a wasm shell over a kernel PTY =="
timeout 90 env -u DISPLAY "$HOST" --xdemo --timeout 22 \
  --server "$EXP/Xvfb.wasm" --client "$EXP/st.wasm" \
  --pty-shell "$EXP/guest-xclient/pty-shell.wasm" \
  --vm-tree "$XFTFONTS" --locale-dir "$VMLOCALE" \
  --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > /tmp/m6st-run.log 2>&1 || true

grep -q "launched xclient0" /tmp/m6st-run.log || { echo "FAIL: st client never launched"; exit 1; }
grep -qiE "Import #0|not an object or function|WebAssembly.Instance" /tmp/m6st-run.log && { echo "FAIL: st.wasm failed to instantiate (unsatisfied imports)"; exit 1; }

python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
core=aa=0
for i in range(0,len(pix),4):
    b,g,r=pix[i],pix[i+1],pix[i+2]
    if r==g==b:
        if r>=0xc0: core+=1               # bright glyph cores (st fg ~0xe5)
        elif 0x10<=r<=0xbf: aa+=1         # antialiased grey edges
print(f"  glyph-core px={core}  antialias-edge px={aa}")
assert core+aa > 120, "no rendered terminal text -> st did not draw the shell's prompt/output"
assert aa > 20, "no antialiased edges -> Xft text rendering path not exercised"
print("PASS: st (wasm) rendered the wasm shell's prompt as antialiased Xft text over the kernel PTY")
PY
rm -f "$FB"
echo "== M6.3 st terminal-emulator PASS =="
