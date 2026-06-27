#!/bin/bash
# Runs INSIDE the sx-wintest container. Expects the repo at /repo, font/locale trees at /tmp/vm*, and an
# output dir at /out. Boots a virtual display, launches the interactive window host, and screenshots it.
set -u
: "${SX_WAIT:=30}"
Xvfb :99 -screen 0 1280x1024x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
export DISPLAY=:99
for i in $(seq 1 50); do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 0.1; done
cd /repo/experiments/wasm-gui
/repo/target/debug/wasm-gui-host --desktop \
  --server ./Xvfb.wasm \
  --client ./twm.wasm \
  --client "./xclock.wasm -analog -update 1 -geometry 150x150+600+40" \
  --client ./guest-xclient/xwin.wasm \
  --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
  --sidecar /repo/target/debug/secure-exec-sidecar \
  -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  >/out/host.log 2>&1 &
HOSTPID=$!
echo "waiting ${SX_WAIT}s for the guest desktop to render into the winit window..."
sleep "$SX_WAIT"
if xwd -root -display :99 -silent 2>/dev/null | convert xwd:- /out/window.png 2>/dev/null; then
  echo "SCREENSHOT OK -> /out/window.png"
else
  echo "SCREENSHOT FAILED"
fi
grep -aE "XTEST input connected|interactive window|launched" /out/host.log | head
kill "$HOSTPID" 2>/dev/null
