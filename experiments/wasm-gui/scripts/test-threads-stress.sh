#!/usr/bin/env bash
# Stress + lifecycle (WASM-THREADS-SPEC.md DoD §9.4): the threads-stress guest does 200 spawn/join
# cycles per run (also proving slot reclamation, since 200 > the cap). Runs REPS times; every run must
# report the exact count. A lost update, a leaked slot, or a hang fails it.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"
SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-stress.wasm"
REPS="${1:-5}"

"$EXP/scripts/build-threads-spike.sh" threads-stress >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar"; exit 1; }

pass=0
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 90 --sidecar "$SIDECAR" 2>&1)"
  line="$(echo "$out" | grep -iE 'counter=|M8-THREADS-STRESS' | tr '\n' ' ')"
  if echo "$out" | grep -q "M8-THREADS-STRESS: PASS"; then pass=$((pass+1)); echo "run $i: PASS  $line"; else echo "run $i: FAIL  $line"; fi
done
echo "reliability: $pass/$REPS"
[ "$pass" -eq "$REPS" ] && { echo "THREADS-STRESS: PASS"; exit 0; } || { echo "THREADS-STRESS: FAIL"; exit 1; }
