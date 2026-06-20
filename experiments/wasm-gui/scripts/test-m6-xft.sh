#!/usr/bin/env bash
# M6.2 (real fonts) test: prove the cross-compiled Xft + fontconfig + freetype stack renders
# ANTIALIASED scalable text through the wasm X server. xftdemo resolves "DejaVu Sans-22" via
# fontconfig (TTF installed into the VM with --vm-tree) and draws a string with XftDrawStringUtf8.
# Asserts the captured framebuffer shows a white window with antialiased (grey-edged) glyphs.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
XFTFONTS="${VMXFT:-/tmp/vmxft}"
FB="$(mktemp /tmp/m6xft-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/guest-xclient/xftdemo.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$XFTFONTS" ] || bash "$EXP/scripts/prepare-xftfonts.sh" "$XFTFONTS" >/dev/null 2>&1

echo "== Xvfb + xftdemo: antialiased Xft/fontconfig text =="
timeout 80 env -u DISPLAY "$HOST" --xdemo --timeout 14 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/guest-xclient/xftdemo.wasm" \
  --vm-tree "$XFTFONTS" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data > /tmp/m6xft-run.log 2>&1 || true

grep -q "XFT:font-opened" /tmp/m6xft-run.log || { echo "FAIL: fontconfig did not resolve a font"; exit 1; }
python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
white=black=gray=0
for i in range(0,len(pix),4):
    b,g,r=pix[i],pix[i+1],pix[i+2]
    if r==g==b==0xff: white+=1
    elif r==g==b and r<0x18: black+=1
    elif abs(r-g)<24 and abs(g-b)<24 and 0x18<=r<=0xe8: gray+=1
print(f"  white(window bg)={white}  black(glyph cores)={black}  gray(antialias edges)={gray}")
assert white > 20000, "xftdemo white window not mapped (wrong frame? rerun without concurrent VMs)"
assert gray > 200, "no antialiased grey edges -> Xft text not rendered with AA"
print("PASS: Xft + fontconfig + freetype render antialiased scalable text in the wasm X server")
PY
rm -f "$FB"
echo "== M6 Xft PASS =="
