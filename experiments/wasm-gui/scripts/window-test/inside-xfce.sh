#!/bin/bash
# Runs INSIDE the sx-wintest container. Proves the multi-app Xfce desktop is interactive IN-BAND: it
# launches the SAME `wasm-gui-host --desktop` path run-desktop.sh uses (full Xfce stack + D-Bus), waits
# for the desktop to render into the winit window, then injects REAL mouse+keyboard into the winit window
# via xdotool (winit -> guest X server over XTEST, exactly like a human at the keyboard) and captures
# before/after screenshots to prove the input round-trip changes the desktop.
#
# Expects: repo at /repo, staged fixture trees at /tmp/vm* (mounted read-only), output dir at /out.
set -u
: "${SX_WAIT:=150}"
: "${W:=800}"; : "${H:=600}"
EXP=/repo/experiments/wasm-gui

Xvfb :99 -screen 0 1280x1024x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
export DISPLAY=:99
for i in $(seq 1 50); do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 0.2; done

cd "$EXP"
# Mirror scripts/run-desktop.sh's launch (full Xfce session), but with binaries already built + fixtures
# pre-staged on the host and mounted at /tmp/vm*. APP_SETTLE_MS gates sequential client launch.
# Client set (WM first). Override with XFCE_CLIENTS="xfwm4 xfce4-panel mousepad xfdesktop". Only the
# earliest-launched ~2-3 heavy GTK clients paint before the single-threaded X server saturates, so order
# the apps you want visible EARLY.
CLIENTS="${XFCE_CLIENTS:-xfwm4 xfce4-panel xfdesktop thunar mousepad}"
CLIENT_ARGS=(); for c in $CLIENTS; do CLIENT_ARGS+=(--client "$EXP/${c}.wasm"); done
LAST_IDX=$(( $(echo $CLIENTS | wc -w) - 1 ))
APP_SETTLE_MS="${APP_SETTLE_MS:-6000}" NO_AT_BRIDGE=1 \
  SX_XTEST_DELAY_S="${SX_XTEST_DELAY_S:-0}" SX_FB_STREAM_MS="${SX_FB_STREAM_MS:-33}" \
  /repo/target/debug/wasm-gui-host --desktop \
  --server "$EXP/Xvfb.wasm" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  "${CLIENT_ARGS[@]}" \
  --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
  --vm-tree /tmp/vmxu3sess-dbus --vm-tree /tmp/vmxu3sess --vm-tree /tmp/vmthemes \
  --vm-tree /tmp/vmxfwm4 --vm-tree /tmp/vmxft --vm-tree /tmp/vmschemas --vm-tree /tmp/vmxkb \
  --sidecar /repo/target/debug/secure-exec-sidecar \
  -- :0 -screen 0 "${W}x${H}x24" -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  >/out/host.log 2>&1 &
HOSTPID=$!

grab() { xwd -root -display :99 -silent 2>/dev/null | convert xwd:- "$1" 2>/dev/null; }

# Fraction of "lit" pixels (0..1) in the WxH window region at the root's top-left. Uses ImageMagick.
lit_frac() { convert "$1" -crop "${W}x${H}+0+0" +repage -threshold 12% -format '%[fx:mean]' info: 2>/dev/null; }

# Wait for the FULL desktop to render: the last client (mousepad = xclient4) must have launched AND real
# content must be lit, up to SX_WAIT seconds. Then settle so it finishes painting before the input test.
echo "waiting up to ${SX_WAIT}s for the full multi-app desktop (through mousepad = xclient4) to render..."
rendered=0
for i in $(seq 1 "$SX_WAIT"); do
  sleep 1
  grab /out/_poll.png || continue
  frac=$(lit_frac /out/_poll.png)
  if grep -qa "launched xclient${LAST_IDX}" /out/host.log 2>/dev/null && awk "BEGIN{exit !(${frac:-0}>0.04)}"; then
    rendered=1; echo "background up after ${i}s (lit ~${frac})"; break
  fi
done
[ "$rendered" = 0 ] && echo "WARN: desktop background not confirmed within ${SX_WAIT}s — proceeding anyway"

# The heavy GTK apps (panel/thunar/mousepad) paint SLOWLY under concurrent load. Capture the paint
# progression so we can see whether they eventually render (timing) or never do (hard ceiling). Distinct
# non-bg colors is a good proxy for "real widgets painted" vs "just the flat xfdesktop background".
: "${PAINT_SETTLE:=150}"
echo "letting the GTK apps paint for up to ${PAINT_SETTLE}s (capturing progression)..."
for t in $(seq 30 30 "$PAINT_SETTLE"); do
  sleep 30
  grab "/out/progress-t${t}.png"
  colors=$(convert "/out/progress-t${t}.png" -crop "${W}x${H}+0+0" +repage -format '%k' info: 2>/dev/null)
  echo "  t+${t}s: distinct colors in window = ${colors:-?}"
done

# BEFORE screenshot.
grab /out/desktop-before.png
echo "=== injecting REAL input into the winit window (xdotool -> winit -> guest X over XTEST) ==="
WIN=$(xdotool search --name "secure-exec" | head -1)
echo "winit window id: ${WIN:-<none>}"
if [ -n "$WIN" ]; then
  eval "$(xdotool getwindowgeometry --shell "$WIN")"   # sets X Y WIDTH HEIGHT
  echo "winit window geom: ${X},${Y} ${WIDTH}x${HEIGHT}"
  xdotool windowactivate --sync "$WIN" 2>/dev/null; sleep 0.5
  # REAL pointer events over the winit window (winit -> guest X server via XTEST, like a human). Click
  # into mousepad's text area to focus it, with a pointer move (cursor round-trip) in between.
  xdotool mousemove $((X + WIDTH/2)) $((Y + HEIGHT/2)); sleep 0.4
  xdotool click 1; sleep 0.8
  xdotool mousemove $((X + WIDTH/3)) $((Y + HEIGHT/3)); sleep 0.4
  xdotool click 1; sleep 1.0
  # REAL key events to the focused winit window -> guest -> the focused editor.
  xdotool type --delay 130 "SecureExec123"; sleep 2.5
fi
# AFTER screenshot.
grab /out/desktop-after.png

# Independent diagnostic: convert the GUEST's own framebuffer (Xvfb_screen0, BGRX) to a PNG. This tells
# us whether the guest desktop actually rendered (guest fb has content) regardless of whether the winit
# window displayed it — separating a guest RENDER bug from a fb-STREAMING-into-winit bug.
GFB=$(ls -1 /tmp/secure-exec-sidecar-shadow-vm-*/data/Xvfb_screen0 2>/dev/null | head -1)
if [ -n "$GFB" ] && [ -s "$GFB" ]; then
  echo "guest framebuffer: $GFB ($(stat -c%s "$GFB") bytes)"
  # Copy the RAW guest fb out so the host can convert it with fb2png.py (reliable header handling) and
  # tell whether the GUEST actually rendered (fb has content) vs the winit DISPLAY being black.
  cp -f "$GFB" /out/guest-fb.bin 2>/dev/null && echo "copied raw guest fb -> /out/guest-fb.bin"
fi

# Report render evidence + whether the input round-trip changed the framebuffer.
grep -aE "XTEST input connected|interactive window|launched xclient|dbus-daemon launched|dbus service" /out/host.log | head -12
if [ -s /out/desktop-before.png ] && [ -s /out/desktop-after.png ]; then
  # Crop to the window region, count differing pixels (ImageMagick AE metric). Fuzz absorbs the caret blink.
  convert /out/desktop-before.png -crop "${W}x${H}+0+0" +repage /out/_b.png
  convert /out/desktop-after.png  -crop "${W}x${H}+0+0" +repage /out/_a.png
  DIFF=$(compare -metric AE -fuzz 12% /out/_b.png /out/_a.png null: 2>&1 | tr -d '[:alpha:] ')
  echo "input round-trip framebuffer delta: ${DIFF:-?} pixels changed in the window region"
  if awk "BEGIN{exit !(${DIFF:-0}>150)}"; then
    echo "IN-BAND INTERACTIVE: YES — real input changed the desktop"
  else
    echo "IN-BAND INTERACTIVE: NO CHANGE DETECTED"
  fi
else
  echo "VERIFY: missing before/after screenshot"
fi
echo "rendered=$rendered"
kill "$HOSTPID" 2>/dev/null
