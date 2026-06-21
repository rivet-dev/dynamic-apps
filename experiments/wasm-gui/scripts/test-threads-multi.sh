#!/usr/bin/env bash
# wasi-threads multi-thread conformance + stress (WASM-THREADS-SPEC.md DoD §9.1/§9.4). Runs the
# threads-multi guest (8 concurrent worker threads, each 2000 atomic increments on a shared counter)
# through the REAL sidecar, REPS times, and requires every run to report exactly N*ITERS with all
# threads spawned + joined. A lost update (broken cross-isolate atomics) or a missing join fails it.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"
SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-multi.wasm"
REPS="${1:-5}"

"$EXP/scripts/build-threads-spike.sh" threads-multi >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar (cargo build -p wasm-gui-host -p secure-exec-sidecar)"; exit 1; }

pass=0
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 40 --sidecar "$SIDECAR" 2>&1)"
  line="$(echo "$out" | grep -iE "spawned=|M8-THREADS-MULTI" | tr '\n' ' ')"
  if echo "$out" | grep -q "M8-THREADS-MULTI: PASS"; then
    pass=$((pass+1)); echo "run $i: PASS  $line"
  else
    echo "run $i: FAIL  $line"
  fi
done
echo "reliability: $pass/$REPS"
[ "$pass" -eq "$REPS" ] && { echo "THREADS-MULTI: PASS"; exit 0; } || { echo "THREADS-MULTI: FAIL"; exit 1; }
