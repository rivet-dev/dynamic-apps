#!/usr/bin/env bash
# libffi-under-threads (WASM-THREADS-SPEC.md DoD R5/§9): concurrent host ffi_call from worker threads
# (the GObject marshal path). 4 workers x 500 ffi_call(add) must all succeed.
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"; REPO_WS="$(cd "$EXP/../.." && pwd)"
HOST="$REPO_WS/target/debug/wasm-gui-host"; SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-ffi.wasm"; REPS="${1:-3}"
"$EXP/scripts/build-threads-spike.sh" threads-ffi >/dev/null || { echo "BUILD FAILED"; exit 1; }
[ -x "$HOST" ] && [ -x "$SIDECAR" ] || { echo "missing host/sidecar"; exit 1; }
pass=0
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 40 --sidecar "$SIDECAR" 2>&1)"
  if echo "$out" | grep -q "M8-THREADS-FFI: PASS"; then pass=$((pass+1)); echo "run $i: PASS  $(echo "$out"|grep -iE 'ok='|tr -d '\n')"; else echo "run $i: FAIL"; fi
done
echo "reliability: $pass/$REPS"
[ "$pass" -eq "$REPS" ] && { echo "THREADS-FFI: PASS"; exit 0; } || { echo "THREADS-FFI: FAIL"; exit 1; }
