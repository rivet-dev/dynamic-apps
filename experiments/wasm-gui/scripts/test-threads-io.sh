#!/usr/bin/env bash
# Worker-thread KERNEL I/O (WASM-THREADS-SPEC.md DoD §9.3): a worker thread makes a real host call
# (write to stdout). Passes only if the worker is a sidecar-mediated session whose host imports route
# to the kernel (sharing the parent's process fd table). Runs REPS times through the real sidecar.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"
SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-io.wasm"
REPS="${1:-5}"

"$EXP/scripts/build-threads-spike.sh" threads-io >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar"; exit 1; }

pass=0
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 30 --sidecar "$SIDECAR" 2>&1)"
  if echo "$out" | grep -q "M8-THREADS-IO: PASS"; then pass=$((pass+1)); echo "run $i: PASS"; else echo "run $i: FAIL"; fi
done
echo "reliability: $pass/$REPS"
[ "$pass" -eq "$REPS" ] && { echo "THREADS-IO: PASS"; exit 0; } || { echo "THREADS-IO: FAIL"; exit 1; }
