#!/usr/bin/env bash
# Record an mp4 of the live wasm desktop: twm managing a libX11 window + a stock xclock (second hand
# ticking), with an XTEST agent gliding the mouse cursor around (and clicking the input target, which
# turns orange). We run the desktop headless, snapshot the X server's framebuffer from the host-backed
# shadow fs at a steady rate, then stitch the raw BGRX frames into an mp4 with ffmpeg.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
FONTS="${VMFONTS:-/tmp/vmfonts}"; LOCALE="${VMLOCALE:-/tmp/vmlocale}"
OUTDIR="${1:-$HOME/tmp/gui-progress}"; OUT="$OUTDIR/wasm-desktop.mp4"
W=640; H=480; FRAMEBYTES=$((W*H*4))
WORK="$(mktemp -d /tmp/deskvid.XXXXXX)"
[ -d "$FONTS" ] || bash "$EXP/scripts/prepare-fonts.sh" >/dev/null 2>&1
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1
mkdir -p "$OUTDIR"

echo "== launching live desktop (twm + ticking analog xclock) =="
# -update 1: redraw every second so the SECOND HAND visibly ticks (xclock's default update is 60s).
timeout 60 env -u DISPLAY "$HOST" --xdemo --timeout 55 \
  --server "$EXP/Xvfb.wasm" \
  --client "$EXP/twm.wasm" \
  --client "$EXP/xclock.wasm -analog -update 1 -geometry 290x290+20+90" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" --sidecar "$SIDECAR" \
  -- :0 -screen 0 ${W}x${H}x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  > /tmp/deskvid-run.log 2>&1 &
HPID=$!

# Wait for the WM + xclock to come up, then poll the framebuffer until the clock has actually drawn
# (lots of white pixels) before we start capturing, so the video doesn't open on a black screen.
echo "== waiting for desktop to come up =="
for _ in $(seq 1 50); do grep -q "launched xclient1" /tmp/deskvid-run.log 2>/dev/null && break; sleep 0.5; done
for _ in $(seq 1 40); do
  FB=$(ls -t /tmp/secure-exec-sidecar-shadow-vm-*/data/Xvfb_screen0 2>/dev/null | head -1)
  if [ -n "$FB" ]; then
    wpx=$(tail -c "$FRAMEBYTES" "$FB" | python3 -c 'import sys;d=sys.stdin.buffer.read();print(sum(1 for i in range(0,len(d),4) if d[i]==0xff and d[i+1]==0xff and d[i+2]==0xff))')
    [ "${wpx:-0}" -gt 20000 ] && { echo "clock drawn (${wpx} white px)"; break; }
  fi
  sleep 0.5
done

echo "== capturing frames =="
NFRAMES=70
for i in $(seq 1 $NFRAMES); do
  FB=$(ls -t /tmp/secure-exec-sidecar-shadow-vm-*/data/Xvfb_screen0 2>/dev/null | head -1)
  [ -n "$FB" ] && tail -c "$FRAMEBYTES" "$FB" > "$(printf '%s/f%04d.raw' "$WORK" "$i")" 2>/dev/null
  sleep 0.18
done
kill "$HPID" 2>/dev/null; wait "$HPID" 2>/dev/null

echo "== encoding mp4 =="
cat "$WORK"/f*.raw > "$WORK/frames.raw"
ffmpeg -y -f rawvideo -pix_fmt bgr0 -s ${W}x${H} -framerate 6 -i "$WORK/frames.raw" \
  -c:v libx264 -pix_fmt yuv420p -movflags +faststart "$OUT" >/tmp/deskvid-ffmpeg.log 2>&1 \
  && echo "wrote $OUT ($(stat -c%s "$OUT") bytes, $(ls "$WORK"/f*.raw | wc -l) frames)" \
  || { echo "ffmpeg FAILED"; tail -5 /tmp/deskvid-ffmpeg.log; }
rm -rf "$WORK"
