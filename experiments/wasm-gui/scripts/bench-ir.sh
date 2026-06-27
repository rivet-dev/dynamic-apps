#!/bin/bash
# bench-ir.sh "<EXTRA_ENV>" <NRUNS>
# Runs the B2 single-app scenario (Xvfb + mousepad) NRUNS times against the CURRENTLY DEPLOYED
# target/debug/{secure-exec-sidecar,wasm-gui-host}. Prints one line per run:
#   RUN <i>: ir=<ms> fp=<ms> render=<fc>fc/<traps>traps/<ok|MISS>
# input→response uses the FULL-BYTE fingerprint (accurate; the old strided default over-reported ~2.6x).
# Build+deploy is the CALLER's job (edit code -> cargo build -> cp to target/debug). This only measures.
set -u
EXP=/home/nathan/secure-exec-wasmgui/experiments/wasm-gui
REPO=/home/nathan/secure-exec-wasmgui
HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
EXTRA="${1:-}"
N="${2:-3}"
cd "$EXP" || exit 3   # the harness resolves --client mousepad.wasm relative to cwd
for r in $(seq 1 "$N"); do
  FB="$(mktemp /tmp/render-fb.XXXXXX.bin)"
  L="$(mktemp /tmp/bench-ir.XXXXXX.log)"
  # shellcheck disable=SC2086
  env -u DISPLAY NO_AT_BRIDGE=1 SECURE_EXEC_FIRSTPAINT=1 SECURE_EXEC_INPUTLATENCY=1 $EXTRA \
    timeout 70 "$HOST" --xdemo --timeout 20 \
    --server "$EXP/Xvfb.wasm" --client "mousepad.wasm" \
    --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
    --vm-tree /tmp/vmxu5sess --vm-tree /tmp/vmicons --vm-tree /tmp/vmxft \
    --vm-tree /tmp/vmschemas --vm-tree /tmp/vmxkb \
    --fb-out "$FB" --sidecar "$SIDECAR" \
    -- :0 -screen 0 800x600x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data \
    > "$L" 2>&1 || true
  ir=$(grep -oE '\[input-response\] [0-9]+ms' "$L" | head -1 | grep -oE '[0-9]+')
  fp=$(grep -oE '\[firstpaint\] [0-9]+ms' "$L" | head -1 | grep -oE '[0-9]+')
  fc=$(grep -c 'fontconfig error' "$L")
  tr=$(grep -cE 'unreachable|FATAL|FUNCTION_SIGNATURE' "$L")
  fb=$(test -s "$FB" && echo ok || echo MISS)
  echo "RUN $r: ir=${ir:-NA}ms fp=${fp:-NA}ms render=${fc}fc/${tr}traps/${fb}"
  # On failure, surface the tail so the caller can diagnose
  if [ -z "${ir:-}" ] || [ -z "${fp:-}" ] || [ "$fb" = "MISS" ]; then
    echo "  --- last 4 log lines (run $r) ---"
    tail -4 "$L" | sed 's/^/  /'
  fi
  rm -f "$FB" "$L"
done
