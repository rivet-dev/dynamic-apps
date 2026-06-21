#!/usr/bin/env bash
# M2.3 (concurrency robustness) test: launch many heavy libX11 clients SIMULTANEOUSLY (no settle /
# ordering hack) and prove they all initialize and render reliably over the single sync-RPC bridge.
# Host `--concurrent` launches every client as soon as the X server is up, back-to-back, instead of the
# default "wait for each to settle" sequencing. twm (the WM) + xclock + xftdemo are started at once;
# twm must reach its event loop and decorate the concurrently-launched windows, xftdemo must open its
# Xft font, nothing may crash, and the framebuffer must show the managed desktop. The M6.4 net.poll
# fairness fix (JAVASCRIPT_NET_POLL_MAX_WAIT 50ms->3ms) is what makes concurrent init non-flaky.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"; VMXFT="${VMXFT:-/tmp/vmxft}"
FB="$(mktemp /tmp/m23-fb.XXXXXX.bin)"

for f in "$HOST" "$SIDECAR" "$EXP/Xvfb.wasm" "$EXP/twm.wasm" "$EXP/xclock.wasm" \
         "$EXP/guest-xclient/xftdemo.wasm"; do
  [ -f "$f" ] || { echo "MISSING: $f"; exit 1; }
done
[ -d "$FONTS" ] || { echo "MISSING fonts $FONTS"; exit 1; }
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1
[ -d "$VMXFT" ] || bash "$EXP/scripts/prepare-xftfonts.sh" "$VMXFT" >/dev/null 2>&1

echo "== concurrent launch: twm + xclock + xftdemo started simultaneously (no settle gating) =="
timeout 110 env -u DISPLAY "$HOST" --xdemo --concurrent --timeout 45 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" --client "$EXP/xclock.wasm" --client "$EXP/guest-xclient/xftdemo.wasm" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --vm-tree "$VMXFT" --fb-out "$FB" --sidecar "$SIDECAR" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts > /tmp/m23-run.log 2>&1 || true

grep -q "launched xclient2.*concurrent" /tmp/m23-run.log || { echo "FAIL: clients not launched concurrently"; exit 1; }
grep -q "TWM:handleevents" /tmp/m23-run.log || { echo "FAIL: twm did not reach its event loop under concurrent init"; exit 1; }
grep -q "XFT:font-opened" /tmp/m23-run.log || { echo "FAIL: xftdemo did not init Xft under concurrent init"; exit 1; }
grep -qE "\(0 failed\)" /tmp/m23-run.log || { echo "FAIL: a concurrently-launched client crashed"; exit 1; }
python3 - "$FB" <<'PY'
import sys
data=open(sys.argv[1],'rb').read(); W,H=640,480; pix=data[len(data)-W*H*4:]
nb=sum(1 for i in range(0,len(pix),4) if pix[i:i+4]!=b'\x00\x00\x00\x00')
white=sum(1 for i in range(0,len(pix),4) if pix[i]==pix[i+1]==pix[i+2]==0xff)
print(f"  nonblack_px={nb} white_px(twm titlebars+clock face)={white}")
assert nb > 45000, "concurrently-launched windows did not render"
assert white > 15000, "no twm decorations / clock face -> WM did not manage the concurrent clients"
print("PASS: 3 heavy libX11 clients launched simultaneously all init + render (twm-managed desktop)")
PY
rm -f "$FB"
echo "== M2.3 concurrent init PASS =="
