#!/usr/bin/env bash
# M8.4: run the real LXDE panel (lxpanel) headless on the wasm X server and capture the framebuffer +
# a FULL (untruncated) X11 wire trace of the lxpanel<->Xvfb exchange. The trace feeds
# scripts/xreassemble.py to localise the first-draw render stall.
#
#   SECURE_EXEC_XTRACE caps per-payload hex; set it large so no first-draw write is truncated.
#   Set WITH_WM=1 to also launch openbox (default off: the stall reproduces 2-party, lxpanel<->Xvfb).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO="$(cd ../.. && pwd)"
FONTS="${VMFONTS:-/tmp/vmfonts}"
LOCALE="${VMLOCALE:-/tmp/vmlocale}"
XFT="${VMXFT:-/tmp/vmxft}"
PANELCFG="${VMLXPANEL:-/tmp/vmlxpanel}"
OUT="${OUT:-/tmp/lxpanel-run.log}"
FB="${FB:-$HOME/tmp/gui-progress/proof-m8.4-lxpanel-renders.png}"
XTRACE_LOG="${XTRACE_LOG:-/tmp/lxpanel-xtrace.log}"
TIMEOUT="${TIMEOUT:-25}"

[ -d "$FONTS" ]  || bash "$EXP/scripts/prepare-fonts.sh"  >/dev/null 2>&1 || true
[ -d "$LOCALE" ] || bash "$EXP/scripts/prepare-locale.sh" "$LOCALE" >/dev/null 2>&1 || true
# Always regenerate the lxpanel config tree (line-based parser needs one setting per line).
bash "$EXP/scripts/prepare-lxpanel.sh" "$PANELCFG" >/dev/null 2>&1 || true

HOST="$REPO/target/debug/wasm-gui-host"
SIDECAR="$REPO/target/debug/secure-exec-sidecar"
[ -x "$HOST" ]    || ( cd "$REPO" && cargo build -p wasm-gui-host )
[ -x "$SIDECAR" ] || ( cd "$REPO" && cargo build -p secure-exec-sidecar --bin secure-exec-sidecar )

for f in "$EXP/Xvfb.wasm" "$EXP/lxpanel.wasm"; do
  [ -f "$f" ] || { echo "MISSING $f — build the wasm artifacts first"; exit 1; }
done

CLIENTS=(--client "$EXP/lxpanel.wasm")
if [ "${WITH_WM:-0}" = "1" ]; then
  [ -f "$EXP/openbox.wasm" ] || { echo "MISSING openbox.wasm"; exit 1; }
  CLIENTS=(--client "$EXP/openbox.wasm" "${CLIENTS[@]}")
fi

VMTREES=()
[ -d "$XFT" ]      && VMTREES+=(--vm-tree "$XFT")
[ -d "$PANELCFG" ] && VMTREES+=(--vm-tree "$PANELCFG")

echo "running lxpanel (WITH_WM=${WITH_WM:-0}) -> fb=$FB  xtrace=$XTRACE_LOG  log=$OUT"
# Capture-cap is intentionally huge so first-draw requests are logged whole for reassembly.
# Forward SECURE_EXEC_STACKDUMP_AFTER_MS (tool #3) when set, to symbolize a livelocked guest stack.
env -u DISPLAY SECURE_EXEC_XTRACE=2000000 \
  ${SECURE_EXEC_STACKDUMP_AFTER_MS:+SECURE_EXEC_STACKDUMP_AFTER_MS=$SECURE_EXEC_STACKDUMP_AFTER_MS} \
  "$HOST" --xdemo \
  --server "$EXP/Xvfb.wasm" \
  "${CLIENTS[@]}" \
  --fb-out "$FB" \
  --fonts-dir "$FONTS" --locale-dir "$LOCALE" \
  "${VMTREES[@]}" \
  --sidecar "$SIDECAR" \
  --timeout "$TIMEOUT" \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data -fp /fonts \
  >"$OUT" 2>&1

# Split the xtrace lines out of the run log for the reassembler.
grep -a "\[xtrace\]" "$OUT" > "$XTRACE_LOG" || true
echo "done. xtrace lines: $(wc -l < "$XTRACE_LOG" 2>/dev/null || echo 0)  (full log: $OUT)"
