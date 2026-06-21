#!/usr/bin/env bash
# M7.5.0 threads spike test (WASM-THREADS-SPEC.md §0 / §9.0). Runs the threaded guest through the REAL
# secure-exec sidecar (V8 isolate path) and classifies the result:
#
#   SPIKE PASS          - real threads: pthread_create succeeds, worker runs, "M8-THREADS: PASS".
#   SPIKE NEGATIVE-GATE - wiring proven: the threaded module instantiates with host-supplied shared
#                         env.memory + a reachable wasi.thread-spawn, _start runs, pthread_create
#                         returns EAGAIN (no real spawn yet -> "M8-THREADS: FAIL"). This is the
#                         expected state until Phase 1 (second-isolate spawn) lands.
#   SPIKE BROKEN        - the module failed to instantiate/run (LinkError, trap, exec rejected).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPO_WS="$(cd "$EXP/../.." && pwd)"   # the jj workspace root (built binaries live here)
HOST="$REPO_WS/target/debug/wasm-gui-host"
SIDECAR="$REPO_WS/target/debug/secure-exec-sidecar"
GUEST="$EXP/guest-xclient/threads-test.wasm"

"$EXP/scripts/build-threads-spike.sh" >/dev/null || { echo "SPIKE BROKEN: build failed"; exit 1; }
[ -x "$HOST" ] || { echo "SPIKE BROKEN: missing $HOST (cargo build -p wasm-gui-host)"; exit 1; }
[ -x "$SIDECAR" ] || { echo "SPIKE BROKEN: missing $SIDECAR (cargo build -p secure-exec-sidecar)"; exit 1; }

OUT="$(env -u DISPLAY "$HOST" --exec --guest "$GUEST" --timeout 30 --sidecar "$SIDECAR" 2>&1)"
echo "--- guest output ---"; echo "$OUT" | grep -iE "pthread_create|M8-THREADS|joined|exec failed|Link|trap|unreachable" | sed 's/^/  /'

if echo "$OUT" | grep -q "M8-THREADS: PASS"; then
  echo "SPIKE PASS: real wasi-threads (pthread_create + join + worker ran)"
  exit 0
fi
if echo "$OUT" | grep -q "pthread_create rc=" && echo "$OUT" | grep -q "M8-THREADS: FAIL"; then
  echo "SPIKE NEGATIVE-GATE: threaded module instantiates with host shared memory + wasi.thread-spawn"
  echo "  reachable; pthread_create returns EAGAIN (real second-isolate spawn pending, Phase 1)."
  exit 0
fi
echo "SPIKE BROKEN: threaded module did not instantiate/run as expected"
exit 1
