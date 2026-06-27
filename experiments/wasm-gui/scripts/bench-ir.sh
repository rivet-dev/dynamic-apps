#!/bin/bash
# bench-ir.sh "<EXTRA_ENV>" <NRUNS>
# Runs the B2 single-app scenario (Xvfb + mousepad) NRUNS times against the CURRENTLY DEPLOYED
# target/debug/{secure-exec-sidecar,wasm-gui-host}. Prints one line per run:
#   RUN <i>: ir=<ms> fp=<ms> render=<fc>fc/<traps>traps/<ok|MISS>
# input→response uses the FULL-BYTE fingerprint (accurate; the old strided default over-reported ~2.6x).
# ir measures a SINGLE keystroke (SECURE_EXEC_IR_TEXT=H) — the true input→response. The old default typed
# "HELLO" (5 chars) and the 15ms-per-char typist pacing sleep runs synchronously BEFORE the detect loop, so
# it baked ~75ms of pacing into ir and could never read below ~75ms regardless of render speed (the "88ms"
# artifact). Override SECURE_EXEC_IR_TEXT to measure a multi-char typing scenario. See §6 RESOLUTION.
# Build+deploy is the CALLER's job (edit code -> cargo build -> cp to target/debug). This only measures.
set -u
# TRUSTWORTHY ir metric (2026-06-30): measure the actual TYPED GLYPH, not pointer/caret artifacts.
#  - IR_TEXT=H: single keystroke.
#  - IR_WARMUP=Hello: warm the editor first so we measure steady-state, not the cold first-edit path.
#  - FB_REGION_BYTES=480000: detect only the top 200 rows where the text line is (row ~70-92) — EXCLUDES
#    the X mouse-cursor sprite (consistently rows ~392-410) that polluted every earlier number.
#  - blink off: staged in the GTK settings.ini fixture (prepare-icons.sh: gtk-cursor-blink=false), so the
#    glyph region only changes on real typing.
#  - IR_POLL_US=500: tighter detect poll (the full-fb 2ms+hash gave ~5ms granularity).
# Damage-confirmed at rows 70-92 = the glyph. True warm single-keystroke ir ~100ms (was mis-read as ~20-88ms).
export SECURE_EXEC_IR_TEXT="${SECURE_EXEC_IR_TEXT:-H}"
export SECURE_EXEC_IR_WARMUP="${SECURE_EXEC_IR_WARMUP:-Hello}"
export SECURE_EXEC_FB_REGION_BYTES="${SECURE_EXEC_FB_REGION_BYTES:-480000}"
export SECURE_EXEC_IR_POLL_US="${SECURE_EXEC_IR_POLL_US:-500}"
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
