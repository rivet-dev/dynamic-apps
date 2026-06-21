#!/usr/bin/env bash
# Aggregate wasi-threads regression + flake gate (WASM-THREADS-SPEC.md DoD §9 / §10): runs the full
# threaded conformance suite, each test repeated, requiring 0 failures. This is the cross-run flake
# gate (a sanitizer build is a noted follow-up). Usage: test-threads-all.sh [reps]
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
REPS="${1:-3}"
SCRIPTS="$EXP/scripts"

declare -a SUITE=(
  "test-threads-spike.sh"        # single thread spawn/join
  "test-threads-multi.sh $REPS"  # 8 concurrent workers + atomics
  "test-threads-io.sh $REPS"     # worker kernel I/O (write)
  "test-threads-stress.sh $REPS" # 200 spawn/join cycles + slot reclamation
  "test-threads-trap.sh"         # worker trap -> VM fault
)

fail=0
for entry in "${SUITE[@]}"; do
  name="${entry%% *}"
  echo "=== $entry ==="
  if "$SCRIPTS/$name" ${entry#* } >/tmp/threads-all-$$.log 2>&1; then
    echo "  $(tail -1 /tmp/threads-all-$$.log)"
  else
    echo "  FAILED:"; tail -4 /tmp/threads-all-$$.log | sed 's/^/    /'; fail=1
  fi
done
# nested (no dedicated script): run inline
echo "=== nested ==="
HOST="$EXP/../../target/debug/wasm-gui-host"; SIDECAR="$EXP/../../target/debug/secure-exec-sidecar"
"$SCRIPTS/build-threads-spike.sh" threads-nested >/dev/null 2>&1
ne_ok=1
for i in $(seq 1 "$REPS"); do
  out="$(env -u DISPLAY "$HOST" --exec --guest "$EXP/guest-xclient/threads-nested.wasm" --timeout 40 --sidecar "$SIDECAR" 2>&1)"
  echo "$out" | grep -q "M8-THREADS-NESTED: PASS" || ne_ok=0
done
[ "$ne_ok" = 1 ] && echo "  nested: PASS ($REPS/$REPS)" || { echo "  nested: FAIL"; fail=1; }

rm -f /tmp/threads-all-$$.log
echo "================================"
[ "$fail" = 0 ] && { echo "THREADS-ALL: PASS"; exit 0; } || { echo "THREADS-ALL: FAIL"; exit 1; }
