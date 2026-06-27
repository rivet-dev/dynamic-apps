#!/usr/bin/env bash
# Phase-0 benchmark suite for the wasm-gui perf work. Each benchmark emits ONE number.
#   B0  raw libX11 window (no GTK)   -> VM + X-server + libX11 first-paint floor (ms)
#   B1  pure GObject compute          -> us/op for new+unref / emit / set+get (CPU-bound check)
#   B2  single GTK app (mousepad)     -> single-app first-paint (ms); target <10s
# (B3 = 5-app Xfce desktop lives in run-desktop.sh / the XU scenarios; measured separately.)
#
# All first-paint numbers come from the host's SECURE_EXEC_FIRSTPAINT framebuffer probe (the moment the
# screen first crosses 2% non-black AFTER its fresh black clear), anchored at X-server launch.
# Usage: bench-suite.sh [b0|b1|b2|all]   (default all)
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"; REPO="$(cd ../.. && pwd)"
HOST="$REPO/target/debug/wasm-gui-host"; SIDECAR="$REPO/target/debug/secure-exec-sidecar"
WHICH="${1:-all}"
# Stage the mandatory fixtures (fonts/locale/xkb/etc.) once.
find /tmp/vmfonts -name '*.ttf' 2>/dev/null | grep -q . || bash scripts/prepare-fonts.sh >/dev/null 2>&1
bash scripts/prepare-xftfonts.sh /tmp/vmxft >/dev/null 2>&1
[ -d /tmp/vmlocale ] || bash scripts/prepare-locale.sh /tmp/vmlocale >/dev/null 2>&1
bash scripts/stage-gschemas.sh /tmp/vmschemas >/dev/null 2>&1
[ -f /tmp/vmxkb/xkb/default.xkm ] || bash scripts/prepare-xkb.sh /tmp/vmxkb >/dev/null 2>&1

firstpaint_run() { # $1 client-spec  $2 extra-vm-trees...
  local client="$1"; shift
  local trees=("$@")
  local fb; fb="$(mktemp /tmp/render-fb.XXXXXX.bin)"
  local log; log="$(mktemp /tmp/bench-fp.XXXXXX.log)"
  local treeargs=(); for t in "${trees[@]}"; do treeargs+=(--vm-tree "$t"); done
  SECURE_EXEC_FIRSTPAINT=1 timeout 90 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout 45 \
    --server "$EXP/Xvfb.wasm" --client "$client" \
    --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale "${treeargs[@]}" \
    --fb-out "$fb" --sidecar "$SIDECAR" -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
    > "$log" 2>&1 || true
  local fp; fp="$(grep -oE '\[firstpaint\] [0-9]+ms' "$log" | head -1)"
  local fc; fc="$(grep -aic 'Fontconfig error' "$log")"
  echo "${fp:-NO-PAINT} (fontconfig_errors=$fc)"
}

if [ "$WHICH" = "b0" ] || [ "$WHICH" = "all" ]; then
  echo "B0 raw-libX11-window first-paint: $(firstpaint_run "guest-xclient/xwin.wasm" /tmp/vmxkb)"
fi
if [ "$WHICH" = "b1" ] || [ "$WHICH" = "all" ]; then
  l="$(mktemp /tmp/bench-b1.XXXXXX.log)"
  timeout 90 env -u DISPLAY "$HOST" --exec --guest "$EXP/bench-gobject.wasm" --timeout 60 --sidecar "$SIDECAR" > "$l" 2>&1 || true
  echo "B1 pure-GObject: $(grep -oE 'BENCH-GOBJECT.*' "$l" | head -1)"
fi
if [ "$WHICH" = "b2" ] || [ "$WHICH" = "all" ]; then
  echo "B2 single-GTK-app(mousepad) first-paint: $(firstpaint_run "mousepad.wasm" /tmp/vmxu5sess /tmp/vmicons /tmp/vmxft /tmp/vmschemas /tmp/vmxkb)   [target <10000ms]"
fi
if [ "$WHICH" = "b3" ] || [ "$WHICH" = "all" ]; then
  # B3 multi-app desktop: WM + several GTK/X apps. firstpaint = first window visible; the
  # [firstpaint-curve] lines show when coverage stabilizes high (all apps painted). APP_SETTLE_MS is
  # the per-app launch-gate; post-L-O/L-P the runtime tolerates a short (2.5s) gate without the
  # concurrent-guest contention collapse that previously forced 9s. Target: painted <30s.
  fb="$(mktemp /tmp/render-fb.XXXXXX.bin)"; log="$(mktemp /tmp/bench-b3.XXXXXX.log)"
  SECURE_EXEC_FIRSTPAINT=1 APP_SETTLE_MS="${APP_SETTLE_MS:-2500}" timeout 150 env -u DISPLAY NO_AT_BRIDGE=1 "$HOST" --xdemo --timeout 110 \
    --server "$EXP/Xvfb.wasm" \
    --client "$EXP/twm.wasm" --client "mousepad.wasm" \
    --client "$EXP/xclock.wasm -analog -update 1 -geometry 150x150+420+40" --client "gtk-hello-mp32.wasm" \
    --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
    --vm-tree /tmp/vmxu5sess --vm-tree /tmp/vmicons --vm-tree /tmp/vmxft --vm-tree /tmp/vmschemas --vm-tree /tmp/vmxkb \
    --fb-out "$fb" --sidecar "$SIDECAR" -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
    > "$log" 2>&1 || true
  fp="$(grep -oE '\[firstpaint\] [0-9]+ms' "$log" | head -1)"
  full="$(grep -oE '\[firstpaint-curve\] [0-9]+ms 6[0-9]' "$log" | head -1 | grep -oE '[0-9]+ms' | head -1)"
  echo "B3 desktop(WM+3 apps) first-window: ${fp:-NO-PAINT}; full-paint(~65%): ${full:-N/A}; launched=$(grep -c 'launched xclient' "$log")   [target painted <30000ms]"
fi
